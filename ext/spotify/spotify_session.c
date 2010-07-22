/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_spotify.h"

#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Loops waiting for a logged in event
// If one is not found after X seconds, a error is returned.
// session->mutex must be locked
int wait_for_logged_in(php_spotify_session *session) {
	struct timespec ts;
	int err = 0;

	assert(session != NULL);

	session->waiting_login++;

	DEBUG_PRINT("wait_for_logged_in (%d)\n", session->waiting_login);

	// Block for a max of 5 seconds
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += SPOTIFY_TIMEOUT;

	while(err == 0) {
		// We first check if we need to process any events
		check_process_events(session);

		// If we are finally logged in break.
		if (session->waiting_login == 0)
			break;

		// Wait until a callback is fired
		err = pthread_cond_timedwait(&session->cv, &session->mutex, &ts);
	}

	return err;
}

// Loops waiting for a logged out event
// session->mutex must be locked
int wait_for_logged_out(php_spotify_session *session) {
	struct timespec ts;
	int err = 0;

	assert(session != NULL);

	DEBUG_PRINT("wait_for_logged_out\n");

	// Block for a max of 5 seconds
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += SPOTIFY_TIMEOUT;

	while(err == 0) {
		// We first check if we need to process any events
		check_process_events(session);

		// If we are finally logged in break.
		if (session->waiting_logout == 0)
			break;

		// Wait until a callback is fired
		err = pthread_cond_timedwait(&session->cv, &session->mutex, &ts);
	}

	return err;
}

/**
 * Session callbacks
 */
static void logged_in (sp_session *session, sp_error error) {
	php_spotify_session *resource = sp_session_userdata(session);
	assert(resource != NULL);

	DEBUG_PRINT("logged_in (%d)\n", error);

	pthread_mutex_lock   (&resource->mutex);

	resource->waiting_login = 0;

	if (error != SP_ERROR_OK)
		resource->last_error = error;

	// Broadcast that we have now logged in (or not)
	wakeup_thread_lock   (resource);
	pthread_mutex_unlock (&resource->mutex);
}

static void logged_out (sp_session *session) {
	php_spotify_session *resource = sp_session_userdata(session);
	assert(resource != NULL);

	DEBUG_PRINT("logged_out\n");

	pthread_mutex_lock   (&resource->mutex);

	resource->waiting_logout = 0;

	// Broadcast that we have now logged out
	wakeup_thread_lock   (resource);
	pthread_mutex_unlock (&resource->mutex);
}

static void metadata_updated (sp_session *session) {
	php_spotify_session *resource = sp_session_userdata(session);
	assert(resource != NULL);

	DEBUG_PRINT("metadata_updated\n");

	// Broadcast that we have some extra metadata
	wakeup_thread(resource);
}

static void connection_error (sp_session *session, sp_error error) {
	DEBUG_PRINT("connection_error\n");
}

static void message_to_user (sp_session *session, const char *message) {
	DEBUG_PRINT("message_to_user: %s", message);
}

static void notify_main_thread (sp_session *session) {
	php_spotify_session *resource = sp_session_userdata(session);
	assert(resource != NULL);

	DEBUG_PRINT("notify_main_thread\n");

	// Signal that an event is queued and needs to be run on the main thread
	pthread_mutex_lock(&resource->mutex);

	resource->events++;

	// Wake up everything (so we get a chance to run the event code)
	wakeup_thread_lock(resource);

	pthread_mutex_unlock(&resource->mutex);
}

static void log_message (sp_session *session, const char *data) {
	DEBUG_PRINT("log_message: %s", data);
}

// Some callbacks we are not interested in, but written down for future's sake
//static int  music_delivery  (sp_session *session, const sp_audioformat *format, const void *frames, int num_frames);
//static void play_token_lost (sp_session *session);
//static void end_of_track    (sp_session *session);
//static void streaming_error (sp_session *session, sp_error error);
//static void userinfo_updated(sp_session *session);

static sp_session_callbacks callbacks = {
	&logged_in,
	&logged_out,
	&metadata_updated,
	&connection_error,
	&message_to_user,
	&notify_main_thread,
	NULL, //&music_delivery,
	NULL, //&play_token_lost,
	&log_message,
	NULL, //&end_of_track,
	NULL, //&streaming_error,
	NULL, //&userinfo_updated,
};

// Creates a session resource object
php_spotify_session * session_resource_create() {

	int err;
	pthread_mutexattr_t attr;
	php_spotify_session * resource = emalloc(sizeof(php_spotify_session));

	resource->session       = NULL;
	resource->events        = 0;
	resource->waiting_login = 0;

	// I hate to do it, but the mutex must be recursive. That's because
	// libspotify sometimes calls the callbacks from the main thread (which causes a deadlock)
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	err = pthread_mutex_init(&resource->mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	if ( err ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error, pthread_mutex_init failed!");
		efree(resource);
		return NULL;
	}

	if ( err = pthread_cond_init (&resource->cv, NULL) ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error, pthread_cond_init failed!");

		pthread_mutex_destroy(&resource->mutex);

		efree(resource);
		return NULL;
	}

	return resource;
}

void session_resource_destory(php_spotify_session *resource) {
	assert(resource != NULL);

	pthread_mutex_destroy(&resource->mutex);
	pthread_cond_destroy (&resource->cv);

	efree(resource);
}

int create_dir(const char *path) {
	struct stat st;

	if (stat(path, &st)) {
		// If the path doesn't exist, lets try and make it
		if (errno == ENOENT) {
			return mkdir(path, 0755);
		}
		return -1;
	}

	return !S_ISDIR(st.st_mode);
}

/* {{{ proto string spotify_session_login(string username, string password, string appkey)
   Logs into spotify and returns a session resource on success
   TODO Ensure there is only one session at a time, and that this session is always for the same user
*/
PHP_FUNCTION(spotify_session_login) {
	sp_session_config config;

	sp_error error;

	php_spotify_session *resource;
	int err;

	char * cache_location;
	char * settings_location;

	char *user, *pass, *key;
	int user_len, pass_len, key_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss",
		&user, &user_len,
		&pass, &pass_len,
		&key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}

	if (user_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "A blank username was given");
		RETURN_FALSE;
	}

	if (pass_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "A blank password was given");
		RETURN_FALSE;
	}

	if (key_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "A blank appkey was given");
		RETURN_FALSE;
	}

	spprintf(&cache_location,    0, "/tmp/libspotify-php/%s/cache/",    user);
	spprintf(&settings_location, 0, "/tmp/libspotify-php/%s/settings/", user);

	// A bug in libspotify means we should create the directories
	if ( create_dir(cache_location) ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to create cache directory \"%s\"", cache_location);
		RETURN_FALSE;
	}

	if ( create_dir(settings_location) ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to create settings directory \"%s\"", settings_location);
		RETURN_FALSE;
	}

	resource = session_resource_create();
	if (resource == NULL) {
		// A error message will be printed in session_resource_create()
		RETURN_FALSE;
	}

	config.api_version       = SPOTIFY_API_VERSION;
	config.cache_location    = cache_location;
	config.settings_location = settings_location;

	config.application_key      = key;
	config.application_key_size = key_len;

	config.user_agent = "libspotify for PHP";
	config.callbacks = &callbacks;

	// Pass the resource to libspotify as userdata
	config.userdata = resource;

	pthread_mutex_lock(&resource->mutex);

	// Create the session
	error = sp_session_init(&config, &resource->session);

	// Ensure we always free these strings
	efree(cache_location);
	efree(settings_location);

	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		goto error;
	}

	// Now try and log in
	error = sp_session_login(resource->session, user, pass);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		goto error;
	}

	err = wait_for_logged_in(resource);
	if (err == ETIMEDOUT) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Timeout while logging in.");
		goto error;
	}

	sp_connectionstate state = sp_session_connectionstate(resource->session);
	if (state != SP_CONNECTION_STATE_LOGGED_IN) {
		SPOTIFY_G(last_error) = resource->last_error;
		goto error;
	}

	pthread_mutex_unlock(&resource->mutex);

	ZEND_REGISTER_RESOURCE(return_value, resource, le_spotify_session);
	return;

error:
	// TODO In the future libspotify will add sp_session_release. Call that here.
	pthread_mutex_unlock(&resource->mutex);
	session_resource_destory(resource);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool spotify_session_logout(resource session)
   Logs out of spotify and returns true on success */
PHP_FUNCTION(spotify_session_logout) {

    php_spotify_session *session;
    zval *zsession;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsession) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify session (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(session, php_spotify_session*, &zsession, -1, PHP_SPOTIFY_SESSION_RES_NAME, le_spotify_session);

    // Always check if some events need processing
    check_process_events(session);

    pthread_mutex_lock(&session->mutex);

	sp_error error = sp_session_logout(session->session);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		pthread_mutex_unlock(&session->mutex);
		RETURN_FALSE;
	}

	// Wait for logout
	wait_for_logged_out(session);

	pthread_mutex_unlock(&session->mutex);

	// TODO In the future libspotify will add sp_session_release. Call that here.

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int spotify_session_connectionstate(resource session)
   Returns the state the session is in. */
PHP_FUNCTION(spotify_session_connectionstate) {
    php_spotify_session *session;
    zval *zsession;
    sp_connectionstate state;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsession) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify session (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(session, php_spotify_session*, &zsession, -1, PHP_SPOTIFY_SESSION_RES_NAME, le_spotify_session);

    // Always check if some events need processing
    check_process_events(session);

    pthread_mutex_lock(&session->mutex);
    state = sp_session_connectionstate(session->session);
    pthread_mutex_unlock(&session->mutex);

	RETURN_LONG( state );
}
/* }}} */

/* {{{ proto string spotify_session_user(resource session)
   Returns the currently logged in user */
PHP_FUNCTION(spotify_session_user) {
    php_spotify_session *session;
    zval *zsession;
    sp_user * user;
    const char *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsession) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify session (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(session, php_spotify_session*, &zsession, -1, PHP_SPOTIFY_SESSION_RES_NAME, le_spotify_session);

    // Always check if some events need processing
    check_process_events(session);

    pthread_mutex_lock(&session->mutex);
    user = sp_session_user(session->session);
    if (user == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "The session is not logged in.");
        pthread_mutex_unlock(&session->mutex);
        RETURN_FALSE;
    }

    name = sp_user_canonical_name(user);
    pthread_mutex_unlock(&session->mutex);

    RETURN_STRING(name, 1);
}
/* }}} */

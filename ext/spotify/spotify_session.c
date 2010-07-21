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

// This must be called with session->mutex held
void check_process_events_lock(php_spotify_session *session) {
	assert(session != NULL);
	DEBUG_PRINT("check_process_events_lock (%d)\n", session->events);

	while (session->events > 0) {
		int timeout = -1;
		DEBUG_PRINT("check_process_events_lock loop(%d)\n", session->events);

		// Don't enter sp_session_process_events locked, otherwise
		// a deadlock might occur
		pthread_mutex_unlock(&session->mutex);
		sp_session_process_events(session->session, &timeout);
		pthread_mutex_lock  (&session->mutex);

		session->events--;
	}

	DEBUG_PRINT("check_process_events_lock end\n");
}

// For some reason libspotify requires events run on the main loop
// So we check for if it needs to be run, and run it.
void check_process_events(php_spotify_session *session) {
	assert(session != NULL);
	DEBUG_PRINT("check_process_events\n");

	pthread_mutex_lock    (&session->mutex);
	check_process_events_lock(session);
	pthread_mutex_unlock  (&session->mutex);

	DEBUG_PRINT("check_process_events end\n");
}

// Loops waiting for a logged in event
// If one is not found after X seconds, a error is returned.
int wait_for_logged_in(php_spotify_session *session) {
	struct timespec ts;
	int err = 0;

	assert(session != NULL);

	DEBUG_PRINT("wait_for_logged_in start\n");

	// Block for a max of 5 seconds
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 10;

	pthread_mutex_lock(&session->mutex);
	while(err == 0) {
		DEBUG_PRINT("wait_for_logged_in loop\n");

		// We first check if we need to process any events
		check_process_events_lock(session);

		// If we are finally logged in break.
		sp_connectionstate state = sp_session_connectionstate(session->session);
		if (state == SP_CONNECTION_STATE_LOGGED_IN)
			break;

		// Wait until a callback is fired
		err = pthread_cond_timedwait(&session->cv, &session->mutex, &ts);
	}
	pthread_mutex_unlock(&session->mutex);

	DEBUG_PRINT("wait_for_logged_in end(%d)\n", err);
	return err;
}

// Must be called with session->mutex held
void wakeup_thread_lock(php_spotify_session *session) {
	assert(session != NULL);
	DEBUG_PRINT("wakeup_thread_lock\n");
	pthread_cond_broadcast(&session->cv);
}

// Wake up any threads waiting for an event
void wakeup_thread(php_spotify_session *session) {
	assert(session != NULL);

	DEBUG_PRINT("wakeup_thread\n");

	pthread_mutex_lock   (&session->mutex);
	wakeup_thread_lock   (session);
	pthread_mutex_unlock (&session->mutex);
}

/**
 * Session callbacks
 */
static void logged_in (sp_session *session, sp_error error) {
	php_spotify_session *resource = sp_session_userdata(session);
	assert(resource != NULL);

	DEBUG_PRINT("logged_in\n");

	// Broadcast that we have now logged in
	wakeup_thread(resource);
}

static void logged_out (sp_session *session) {
	DEBUG_PRINT("logged_out\n");
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
};

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
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "A blank username was given");
		RETURN_FALSE;
	}

	if (pass_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "A blank password was given");
		RETURN_FALSE;
	}

	if (key_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "A blank appkey was given");
		RETURN_FALSE;
	}

	// Create the resource object
	resource = emalloc(sizeof(php_spotify_session));
	resource->session = NULL;
	resource->events  = 0;

	if ( err = pthread_mutex_init(&resource->mutex, NULL) ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error, pthread_mutex_init failed!");
		RETURN_FALSE;
	}

	if ( err = pthread_cond_init (&resource->cv, NULL) ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error, pthread_cond_init failed!");
		RETURN_FALSE;
	}

	spprintf(&cache_location,    0, "/tmp/libspotify-php/%s/cache/",    user);
	spprintf(&settings_location, 0, "/tmp/libspotify-php/%s/settings/", user);

	config.api_version       = SPOTIFY_API_VERSION;
	config.cache_location    = cache_location;
	config.settings_location = settings_location;

	config.application_key      = key;
	config.application_key_size = key_len;

	config.user_agent = "libspotify for PHP";
	config.callbacks = &callbacks;

	// Pass the resource to libspotify as userdata
	config.userdata = resource;

	// Create the session
	error = sp_session_init(&config, &resource->session);

	// Ensure we always free these strings
	efree(cache_location);
	efree(settings_location);

	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		RETURN_FALSE;
	}

	// Now try and log in
	error = sp_session_login(resource->session, user, pass);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		// TODO In the future libspotify will add sp_session_release. Call that here.
		RETURN_FALSE;
	}

	err = wait_for_logged_in(resource);
	if (err == ETIMEDOUT) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Timeout while logging in.");
		RETURN_FALSE;
	}

	ZEND_REGISTER_RESOURCE(return_value, resource, le_spotify_session);
}
/* }}} */

/*
 * pthread_mutex_destroy(&resource->mutex)
 * pthread_cond_destroy(&resource->cv)
 */

/* {{{ proto bool spotify_session_logout(resource session)
   Logs out of spotify and returns true on success */
PHP_FUNCTION(spotify_session_logout) {

	sp_session *session;

	sp_error error = sp_session_logout(session);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		RETURN_FALSE;
	}

	// Wait for logout

	// TODO In the future libspotify will add sp_session_release. Call that here.

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int spotify_session_connectionstate(resource session)
   Returns the state the session is in */
PHP_FUNCTION(spotify_session_connectionstate) {

}
/* }}} */

/* {{{ proto string spotify_session_user(resource session)
   Returns the currently logged in user */
PHP_FUNCTION(spotify_session_user) {
    php_spotify_session *session;
    zval *zsession;
    sp_user * user;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsession) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify session (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(session, php_spotify_session*, &zsession, -1, PHP_SPOTIFY_SESSION_RES_NAME, le_spotify_session);

    user = sp_session_user(session->session);
    if (user == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "The session is not logged in.");
        RETURN_FALSE;
    }

    RETURN_STRING(sp_user_canonical_name(user), 1);
}
/* }}} */

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

/**
 * Session callbacks
 */

static void logged_in          (sp_session *session, sp_error error);
static void logged_out         (sp_session *session);
static void metadata_updated   (sp_session *session);
static void connection_error   (sp_session *session, sp_error error);
static void message_to_user    (sp_session *session, const char *message);
static void notify_main_thread (sp_session *session);
static int  music_delivery     (sp_session *session, const sp_audioformat *format, const void *frames, int num_frames);
static void play_token_lost    (sp_session *session);
static void log_message        (sp_session *session, const char *data);
static void end_of_track       (sp_session *session);

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

static void logged_in (sp_session *session, sp_error error) {
	php_printf("logged_in\n");
}

static void logged_out (sp_session *session) {
	php_printf("logged_out\n");
}

static void metadata_updated (sp_session *session) {
	php_printf("metadata_updated\n");
}

static void connection_error (sp_session *session, sp_error error) {
	php_printf("connection_error\n");
}

static void message_to_user (sp_session *session, const char *message) {
	php_printf("message_to_user(%s)\n", message);
}

static void notify_main_thread (sp_session *session) {
	php_printf("notify_main_thread\n");
}

static void log_message (sp_session *session, const char *data) {
	php_printf("log_message(%s)\n", data);
}


/* {{{ proto string spotify_session_login(string username, string password, string appkey)
   Logs into spotify and returns a session resource on success */
PHP_FUNCTION(spotify_session_login) {
	sp_session_config config;
	sp_session *session;

	sp_error error;

	php_spotify_session *resource;

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

	spprintf(&cache_location,    0, "/tmp/libspotify-php/%s/cache/",    user);
	spprintf(&settings_location, 0, "/tmp/libspotify-php/%s/settings/", user);

	config.api_version       = SPOTIFY_API_VERSION;
	config.cache_location    = cache_location;
	config.settings_location = settings_location;

	config.application_key      = key;
	config.application_key_size = key_len;

	config.user_agent = "libspotify for PHP";
	config.callbacks = &callbacks;

	// Create the session
	error = sp_session_init(&config, &session);

	// Ensure we always free these strings
	efree(cache_location);
	efree(settings_location);

	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		RETURN_FALSE;
	}

	// Now try and log in
	error = sp_session_login(session, user, pass);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		// TODO In the future libspotify will add sp_session_release. Call that here.
		RETURN_FALSE;
	}

	// TODO Wait for login

	resource = emalloc(sizeof(php_spotify_session));
	resource->session = session;

	ZEND_REGISTER_RESOURCE(return_value, resource, le_spotify_session);
}
/* }}} */

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

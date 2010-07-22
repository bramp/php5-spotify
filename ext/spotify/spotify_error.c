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

/* {{{ proto int spotify_last_error([session])
   Returns the last global error code or the last session specific one. */
PHP_FUNCTION(spotify_last_error) {
    zval *zsession = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|r", &zsession) == FAILURE) {
        RETURN_FALSE;
    }

    if (zsession) {
		sp_error last_error;
		php_spotify_session *session;

		// Check its a spotify session (otherwise RETURN_FALSE)
		ZEND_FETCH_RESOURCE(session, php_spotify_session*, &zsession, -1, PHP_SPOTIFY_SESSION_RES_NAME, le_spotify_session);

		// Fetch the error in a thread safe way
		pthread_mutex_lock(&session->mutex);
		last_error = session->last_error;
		pthread_mutex_unlock(&session->mutex);

		// Return the session error
		RETURN_LONG( last_error );
    }

    // Return the global error
	RETURN_LONG( SPOTIFY_G(last_error) );
}
/* }}} */

/* {{{ proto string spotify_error_message(int error)
   Returns a text string for the error code */
PHP_FUNCTION(spotify_error_message) {
	long error;
	const char *msg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &error) == FAILURE) {
		RETURN_FALSE;
	}

	msg = sp_error_message((sp_error)error);

	RETURN_STRING(msg, 1);
}
/* }}} */

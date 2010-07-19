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

/* {{{ proto int spotify_last_error()
   Returns the last error code */
PHP_FUNCTION(spotify_last_error) {
	RETURN_LONG( SPOTIFY_G(last_error) );
}
/* }}} */

/* {{{ proto string spotify_session_login(int error)
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

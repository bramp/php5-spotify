/**
 * A PHP extension to wrap some of libspotify
 * Copyright 2010
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 *
 * TODOs:
 *  Add ARG_INFO infomation, so reflexition works with the extension
 *  Check ZEND_FETCH_RESOURCE actually returns if the wrong resource is given
 *
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_spotify.h"

#define PHP_SPOTIFY_VERSION "0.1"

ZEND_DECLARE_MODULE_GLOBALS(spotify)

int le_spotify_session;
int le_spotify_playlist;

/* {{{ spotify_functions[]
 */
const zend_function_entry spotify_functions[] = {
	PHP_FE(spotify_session_login,  NULL)
	PHP_FE(spotify_session_logout, NULL)
	PHP_FE(spotify_session_connectionstate, NULL)
	PHP_FE(spotify_session_user, NULL)

	PHP_FE(spotify_playlist_create,     NULL)
	PHP_FE(spotify_playlist_name,       NULL)
	PHP_FE(spotify_playlist_rename,     NULL)
	PHP_FE(spotify_playlist_add_tracks, NULL)
//	PHP_FE(spotify_playlist_get_tracks, NULL)
	PHP_FE(spotify_playlist_uri,        NULL)

	PHP_FE(spotify_last_error, NULL)
	PHP_FE(spotify_error_message, NULL)

	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ spotify_module_entry
 */
zend_module_entry spotify_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"spotify",
	spotify_functions,
	PHP_MINIT(spotify),
	PHP_MSHUTDOWN(spotify),
	PHP_RINIT(spotify),
	PHP_RSHUTDOWN(spotify),
	PHP_MINFO(spotify),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_SPOTIFY_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SPOTIFY
ZEND_GET_MODULE(spotify)
#endif

// Some dtors
void php_spotify_session_p_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
void php_spotify_playlist_dtor (zend_rsrc_list_entry *rsrc TSRMLS_DC);


// This are blocked on when waiting for a callback (such as login).
// In future we could split these into multiple mutex/cv pairs, one for each type of
// callback.
pthread_mutex_t request_mutex;  // Mutex to lock on when waiting for a (libspotify) callback
pthread_cond_t  request_cv;     // Conditional var for blocking for a callback

void request_lock() {
	DEBUG_PRINT("request_lock\n");
	pthread_mutex_lock ( &request_mutex );
}

void request_unlock() {
	DEBUG_PRINT("request_unlock\n");
	pthread_mutex_unlock ( &request_mutex );
}

void request_wake_lock() {
	DEBUG_PRINT("request_wake_lock\n");
	pthread_cond_broadcast( &request_cv );
}

void request_wake() {
	request_lock();
	request_wake_lock();
	request_unlock();
}

// Must be called with request_lock() held
int request_sleep_lock(const struct timespec *restrict abstime) {
	if (abstime)
		return pthread_cond_timedwait(&request_cv, &request_mutex, abstime);
	return pthread_cond_wait(&request_cv, &request_mutex);
}

int request_sleep(const struct timespec *restrict abstime) {
	int ret;
	request_lock();
	ret = request_sleep_lock(abstime);
	request_unlock();
	return ret;
}

int request_init() {
	int err;

	err = pthread_mutex_init(&request_mutex, NULL);
	if ( err ) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error, pthread_mutex_init() failed!");
		return FAILURE;
	}

	err = pthread_cond_init (&request_cv, NULL);
	if ( err ) {
		pthread_mutex_destroy(&request_mutex);
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error, pthread_cond_init() failed!");
		return FAILURE;
	}

	return SUCCESS;
}

void request_shutdown() {
	DEBUG_PRINT("request_shutdown\n");

	pthread_mutex_destroy(&request_mutex);
	pthread_cond_destroy (&request_cv);
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(spotify)
{
	DEBUG_PRINT("PHP_MINIT_FUNCTION\n");

    if (request_init() == FAILURE) {
    	return FAILURE;
    }

	REGISTER_LONG_CONSTANT("SPOTIFY_CONNECTION_STATE_LOGGED_OUT",   SP_CONNECTION_STATE_LOGGED_OUT,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_CONNECTION_STATE_LOGGED_IN",    SP_CONNECTION_STATE_LOGGED_IN,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_CONNECTION_STATE_DISCONNECTED", SP_CONNECTION_STATE_DISCONNECTED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_CONNECTION_STATE_UNDEFINED",    SP_CONNECTION_STATE_UNDEFINED,    CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_OK",                  SP_ERROR_OK,                  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_BAD_API_VERSION",     SP_ERROR_BAD_API_VERSION,     CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_TRACK_NOT_PLAYABLE",  SP_ERROR_TRACK_NOT_PLAYABLE,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_RESOURCE_NOT_LOADED", SP_ERROR_RESOURCE_NOT_LOADED, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_BAD_APPLICATION_KEY",      SP_ERROR_BAD_APPLICATION_KEY,      CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_BAD_USERNAME_OR_PASSWORD", SP_ERROR_BAD_USERNAME_OR_PASSWORD, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_USER_BANNED",              SP_ERROR_USER_BANNED,              CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_UNABLE_TO_CONTACT_SERVER", SP_ERROR_UNABLE_TO_CONTACT_SERVER, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_CLIENT_TOO_OLD",   SP_ERROR_CLIENT_TOO_OLD,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_OTHER_PERMANENT",  SP_ERROR_OTHER_PERMANENT,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_BAD_USER_AGENT",   SP_ERROR_BAD_USER_AGENT,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_MISSING_CALLBACK", SP_ERROR_MISSING_CALLBACK, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_INVALID_INDATA",     SP_ERROR_INVALID_INDATA,     CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_INDEX_OUT_OF_RANGE", SP_ERROR_INDEX_OUT_OF_RANGE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_USER_NEEDS_PREMIUM", SP_ERROR_USER_NEEDS_PREMIUM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_OTHER_TRANSIENT",    SP_ERROR_OTHER_TRANSIENT,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SPOTIFY_ERROR_IS_LOADING",         SP_ERROR_IS_LOADING,         CONST_CS | CONST_PERSISTENT);

	le_spotify_session  = zend_register_list_destructors_ex(
			NULL, php_spotify_session_p_dtor, PHP_SPOTIFY_SESSION_RES_NAME,  module_number);

	le_spotify_playlist = zend_register_list_destructors_ex(
			php_spotify_playlist_dtor, NULL, PHP_SPOTIFY_PLAYLIST_RES_NAME, module_number);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(spotify)
{
	DEBUG_PRINT("PHP_MSHUTDOWN_FUNCTION\n");

	request_shutdown();

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(spotify)
{
	int err;

	DEBUG_PRINT("PHP_RINIT_FUNCTION\n");

	SPOTIFY_G(last_error) = SP_ERROR_OK;

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(spotify)
{
	DEBUG_PRINT("PHP_RSHUTDOWN_FUNCTION\n");
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(spotify)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Spotify support", "enabled");
	php_info_print_table_row(2, "Spotify version", PHP_SPOTIFY_VERSION);
	php_info_print_table_row(2, "Author",          "Andrew Brampton");
	php_info_print_table_end();
}
/* }}} */


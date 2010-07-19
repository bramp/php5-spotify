/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "spotify.h" // Our own header

ZEND_DECLARE_MODULE_GLOBALS(spotify)

static function_entry spotify_functions[] = {
	PHP_FE(spotify_session_login,  NULL)
	PHP_FE(spotify_session_logout, NULL)

	PHP_FE(spotify_playlist_create,     NULL)
	PHP_FE(spotify_playlist_name,       NULL)
	PHP_FE(spotify_playlist_rename,     NULL)
	PHP_FE(spotify_playlist_add_tracks, NULL)
//	PHP_FE(spotify_playlist_get_tracks, NULL)
	PHP_FE(spotify_playlist_uri,        NULL)

	PHP_FE(spotify_last_error, NULL)

	{NULL, NULL, NULL}
};

zend_module_entry spotify_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_SPOTIFY_EXTNAME,
	spotify_functions,
	PHP_MINIT(spotify),
	PHP_MSHUTDOWN(spotify),
	PHP_RINIT(spotify),
	PHP_RSHUTDOWN(spotify),
	NULL,
#if ZEND_MODULE_API_NO >= 20010901
	PHP_SPOTIFY_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SPOTIFY
ZEND_GET_MODULE(spotify)
#endif

static void spotify_init_globals(zend_spotify_globals *spotify_globals) {}

// Called at module init
PHP_MINIT_FUNCTION(spotify) {
	ZEND_INIT_MODULE_GLOBALS(spotify, spotify_init_globals, NULL);
	return SUCCESS;
}

// Called at module shutdown
PHP_MSHUTDOWN_FUNCTION(spotify) {
	return SUCCESS;
}

// Called at request init
PHP_RINIT_FUNCTION(spotify) {
	SPOTIFY_G(last_error) = SP_ERROR_OK;

	return SUCCESS;
}

// Called at request shutdown
PHP_RSHUTDOWN_FUNCTION(spotify) {
	return SUCCESS;
}

/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 */
#ifndef PHP_SPOTIFY_H
#define PHP_SPOTIFY_H 1

#ifdef ZTS
# include "TSRM.h"
#endif

#include <libspotify/api.h>

#define PHP_SPOTIFY_VERSION "0.1"
#define PHP_SPOTIFY_EXTNAME "spotify"

ZEND_BEGIN_MODULE_GLOBALS(spotify)
    sp_error last_error; // The error code for the last error
ZEND_END_MODULE_GLOBALS(spotify)

#ifdef ZTS
# define SPOTIFY_G(v) TSRMG(spotify_globals_id, zend_spotify_globals *, v)
#else
# define SPOTIFY_G(v) (spotify_globals.v)
#endif

PHP_MINIT_FUNCTION(spotify);
PHP_RINIT_FUNCTION(spotify);

PHP_MSHUTDOWN_FUNCTION(spotify);
PHP_RSHUTDOWN_FUNCTION(spotify);

PHP_FUNCTION(spotify_session_login);
PHP_FUNCTION(spotify_session_logout);

PHP_FUNCTION(spotify_playlist_create);
PHP_FUNCTION(spotify_playlist_name);
PHP_FUNCTION(spotify_playlist_rename);
PHP_FUNCTION(spotify_playlist_add_tracks);
PHP_FUNCTION(spotify_playlist_uri);

PHP_FUNCTION(spotify_last_error);

extern zend_module_entry spotify_module_entry;
#define phpext_spotify_ptr &spotify_module_entry;

#endif

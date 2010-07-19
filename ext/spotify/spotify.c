/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 *
 * TODOs:
 *  Add ARG_INFO infomation, so reflexition works with the extension
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_spotify.h"

#define PHP_SPOTIFY_VERSION "0.1"

ZEND_DECLARE_MODULE_GLOBALS(spotify)

/* True global resources - no need for thread safety here */
//static int le_spotify;

/* {{{ spotify_functions[]
 *
 * Every user visible function must have an entry in spotify_functions[].
 */
const zend_function_entry spotify_functions[] = {
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

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(spotify)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(spotify)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(spotify)
{
	SPOTIFY_G(last_error) = SP_ERROR_OK;
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(spotify)
{
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

/* {{{ proto string confirm_spotify_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_spotify_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "spotify", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */

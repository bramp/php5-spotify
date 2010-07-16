#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "spotify.h"
#include "php_spotify.h"

static function_entry spotify_functions[] = {
    PHP_FE(hello_world, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry spotify_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_HELLO_WORLD_EXTNAME,
    hello_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_SPOTIFY_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_HELLO
ZEND_GET_MODULE(spotify)
#endif

PHP_FUNCTION(hello_world)
{
    RETURN_STRING("Hello World", 1);
}

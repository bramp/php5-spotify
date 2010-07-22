/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 */

#ifndef PHP_SPOTIFY_H
#define PHP_SPOTIFY_H

#include <pthread.h>
#include <libspotify/api.h>

extern zend_module_entry spotify_module_entry;
#define phpext_spotify_ptr &spotify_module_entry

#ifdef PHP_WIN32
#	define PHP_SPOTIFY_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_SPOTIFY_API __attribute__ ((visibility("default")))
#else
#	define PHP_SPOTIFY_API
#endif

#define DEBUG_SPOTIFY

#ifdef DEBUG_SPOTIFY
# define DEBUG_PRINT(f, s...) php_printf("%X ", pthread_self()); php_printf(f, ## s)
#else
# define DEBUG_PRINT(f)
#endif

#ifdef ZTS
# include "TSRM.h"
#endif

// The number of seconds to wait for pending events
// such as login, or commits to playlist changes
#define SPOTIFY_TIMEOUT 10

#define PHP_SPOTIFY_SESSION_RES_NAME "Spotify Session"
typedef struct _php_spotify_session {
	sp_session *session;    // Spotify session
	sp_error last_error;    // The last error that has occured (used for handing back to main thread)

	char *user;             // The username this session is for

	pthread_mutex_t mutex;  // Mutex to lock on when blocking and used to make libspotify threadsafe
	pthread_cond_t  cv;     // Conditional var for blocking

	int events;             // The number of process events waiting
	int waiting_login;      // The number of waiters on the login callback
	int waiting_logout;     // The number of waiters on the logout callback
} php_spotify_session;

#define PHP_SPOTIFY_PLAYLIST_RES_NAME "Spotify Playlist"
typedef struct _php_spotify_playlist {
	php_spotify_session *session;  // Parent session
	sp_playlist *playlist;         // Spotify playlist

	// In future we could use mutex per playlist
	// That would reduce surpoius wakeups, but would make threading more complex
	// as libspoity only allows one thread using its API at a time.
	//pthread_mutex_t mutex;         // Mutex to lock on when blocking
	//pthread_cond_t  cv;            // Conditional var for blocking
} php_spotify_playlist;

extern int le_spotify_session;
extern int le_spotify_playlist;

// Some dtors
void php_spotify_session_dtor  (zend_rsrc_list_entry *rsrc TSRMLS_DC);
void php_spotify_session_p_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
void php_spotify_playlist_dtor (zend_rsrc_list_entry *rsrc TSRMLS_DC);

// Some common functions
void check_process_events(php_spotify_session *session);
void wakeup_thread_lock(php_spotify_session *session);
void wakeup_thread(php_spotify_session *session);

// The module functions
PHP_MINIT_FUNCTION(spotify);
PHP_RINIT_FUNCTION(spotify);
PHP_MSHUTDOWN_FUNCTION(spotify);
PHP_RSHUTDOWN_FUNCTION(spotify);
PHP_MINFO_FUNCTION(spotify);

// The PHP functions
PHP_FUNCTION(spotify_session_login);
PHP_FUNCTION(spotify_session_logout);
PHP_FUNCTION(spotify_session_connectionstate);
PHP_FUNCTION(spotify_session_user);

PHP_FUNCTION(spotify_playlist_create);
PHP_FUNCTION(spotify_playlist_name);
PHP_FUNCTION(spotify_playlist_rename);
PHP_FUNCTION(spotify_playlist_add_tracks);
PHP_FUNCTION(spotify_playlist_uri);

PHP_FUNCTION(spotify_last_error);
PHP_FUNCTION(spotify_error_message);

ZEND_BEGIN_MODULE_GLOBALS(spotify)
	sp_error last_error; // The error code for the last error
ZEND_END_MODULE_GLOBALS(spotify)

PHPAPI ZEND_EXTERN_MODULE_GLOBALS(spotify)

#ifdef ZTS
# define SPOTIFY_G(v) TSRMG(spotify_globals_id, zend_spotify_globals *, v)
#else
# define SPOTIFY_G(v) (spotify_globals.v)
#endif

#endif	/* PHP_SPOTIFY_H */

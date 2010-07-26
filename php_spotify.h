/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 */

#ifndef PHP_SPOTIFY_H
#define PHP_SPOTIFY_H

#include <pthread.h>
#include <semaphore.h>
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
# define DEBUG_PRINT(f, s...) php_printf("%X ", (unsigned int)pthread_self()); php_printf(f, ## s)
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

	int events;             // The number of process events waiting
	int waiting_login;      // The number of waiters on the login callback
	int waiting_logout;     // The number of waiters on the logout callback
} php_spotify_session;

#define PHP_SPOTIFY_PLAYLIST_RES_NAME "Spotify Playlist"
typedef struct _php_spotify_playlist {
	php_spotify_session *session;  // Parent session
	sp_playlist *playlist;         // Spotify playlist
} php_spotify_playlist;

extern int le_spotify_session;
extern int le_spotify_playlist;

// Some dtors
void php_spotify_session_dtor  (zend_rsrc_list_entry *rsrc TSRMLS_DC);
void php_spotify_session_p_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
void php_spotify_playlist_dtor (zend_rsrc_list_entry *rsrc TSRMLS_DC);

// Some common functions
void wakeup_thread_lock(php_spotify_session *session);
void wakeup_thread(php_spotify_session *session);
int session_logged_in(php_spotify_session *session);

// Request locking functions
// Used when the request has to wait for a libspotify callback.
void request_lock();
void request_unlock();
void request_wake();
int  request_sleep_lock(const struct timespec *restrict abstime);
int  request_sleep(const struct timespec *restrict abstime);

// Macros for locking - They are macros so the __FILE__ define works in a useful way.
// Eventually use res->mutex, but at the moment just use the global session_mutex;
#ifndef DEBUG_SPOTIFY
# define session_lock(res)   pthread_mutex_lock  (&session_mutex); php_printf("%X LOCK %s:%d\n",   (unsigned int)pthread_self(),  __FILE__, __LINE__);
# define session_unlock(res) pthread_mutex_unlock(&session_mutex); php_printf("%X UNLOCK %s:%d\n", (unsigned int)pthread_self(),  __FILE__, __LINE__);
#else
# define session_lock(res)   pthread_mutex_lock  (&session_mutex);
# define session_unlock(res) pthread_mutex_unlock(&session_mutex);
#endif

// The global vars
extern sp_session *    session;        // The single session
extern php_spotify_session * session_resource;
extern pthread_t       session_thread; // The thread handling notifiy_main_thread events
extern pthread_mutex_t session_mutex;  // This will be moved into the session resource
extern sem_t session_sem;              // But they ensure only one thread uses the session at a time.

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

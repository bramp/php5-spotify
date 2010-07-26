/**
 * A PHP extension to wrap some of libspotify
 * By Andrew Brampton <me@bramp.net> 2010
 *    for outsideline.co.uk
 */
#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_spotify.h"

#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// This basicaly loops waiting for notify_main_threads
static void *session_main_thread(void *data) {

	php_spotify_session * resource = data;

	DEBUG_PRINT("session_main_thread start\n");

	// Wait until the session is good
	while(resource->running) {
		int timeout = -1; // TODO use the timeout

		// Read it into a seperate var so it doesn't get changed underneath us
		sp_session * session = resource->session;
		if (session != NULL) {

			session_lock(resource);
			sp_session_process_events(session, &timeout);
			session_unlock(resource);

			DEBUG_PRINT("session_main_thread %d\n", timeout);
		}

		// Wait until a callback is fired
		sem_wait(&resource->sem);

		DEBUG_PRINT("session_main_thread wakeup\n");
	}

	DEBUG_PRINT("session_main_thread end\n");

	pthread_exit(NULL);
}

void poke_main_thread(php_spotify_session *resource) {
	assert(resource != NULL);
	sem_post(&resource->sem);
}

// Creates a session resource object
php_spotify_session * session_resource_create(char *user) {
	int err;
	const char *errmsg;
	php_spotify_session * resource;
	pthread_mutexattr_t attr;

	DEBUG_PRINT("session_resource_create\n");

	resource = pemalloc(sizeof(php_spotify_session), 1);
	resource->session       = NULL;
	resource->user          = pestrdup(user, 1);
	resource->waiting_login = 0;
	resource->waiting_logout= 0;
	resource->running       = 1;

	err = sem_init(&resource->sem, 0, 0);
	if ( err ) {
		errmsg = "Internal error, sem_init() failed!";
		goto error_sem;
	}

	// TODO retest this assumption
	// I hate to do it, but the mutex must be recursive. That's because
	// libspotify sometimes calls the callbacks from the main thread (which causes a deadlock)
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	err = pthread_mutex_init(&resource->mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	if ( err ) {
		errmsg = "Internal error, pthread_mutex_init() failed!";
		goto error_mux;
	}

	// Create a single "main" thread, used to handle sp_session_process_events()
	err = pthread_create(&resource->thread, NULL, session_main_thread, resource);
    if ( err ) {
    	errmsg = "Internal error, pthread_create() failed!";
    	goto error_thread;
    }

	return resource;

error_thread:
	pthread_mutex_destroy(&resource->mutex);

error_mux:
	sem_destroy (&resource->sem);

error_sem:
	pefree(resource->user, 1);
	pefree(resource, 1);

	php_error_docref(NULL TSRMLS_CC, E_ERROR, errmsg);

	return NULL;
}

void session_resource_destory(php_spotify_session *resource) {

	DEBUG_PRINT("session_resource_destory\n");

	if (resource == NULL)
		return;

    // Kill the thread
	session_lock(resource);
	resource->running = 0;
	session_unlock(resource);

	poke_main_thread(resource);
	pthread_join(resource->thread, NULL);

	// Now clean up the memory
	pthread_mutex_destroy(&resource->mutex);
	sem_destroy (&resource->sem);

	pefree(resource->user, 1);
	pefree(resource, 1);
}

void php_spotify_session_p_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
	php_spotify_session *session;

	DEBUG_PRINT("php_spotify_session_p_dtor\n");

	session = (php_spotify_session*)rsrc->ptr;

    if (session) {
    	int needsUnlock = 1;

        session_lock(session);

        // If we are logged in, log us out (and therefore save any remaining state).
        if (session_logged_in(session)) {

			sp_error error = sp_session_logout(session->session);

			if (error == SP_ERROR_OK) {
				// We don't need to hold the session lock when we enter the wait function.
				session_unlock(session);
				needsUnlock = 0;

				wait_for_logged_out(session);
			}
        }

        if (needsUnlock)
        	session_unlock(session);

        session_resource_destory(session);
    }

    DEBUG_PRINT("php_spotify_session_p_dtor end\n");
}

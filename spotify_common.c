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

#include <assert.h>
#include <pthread.h>

// This must be called with session->mutex held
void check_process_events(php_spotify_session *session) {
	assert(session != NULL);
	//DEBUG_PRINT("check_process_events (%d)\n", session->events);

	while (session->events > 0) {
		int timeout = -1;

		// Don't enter sp_session_process_events locked, otherwise
		// a deadlock might occur
		//pthread_mutex_unlock(&session->mutex);
		sp_session_process_events(session->session, &timeout);
		//pthread_mutex_lock  (&session->mutex);

		session->events--;
	}

	//DEBUG_PRINT("check_process_events end\n");
}

// Must be called with session->mutex held
void wakeup_thread_lock(php_spotify_session *session) {
	assert(session != NULL);
	//DEBUG_PRINT("wakeup_thread_lock\n");
	pthread_cond_broadcast(&session->cv);
}

// Wake up any threads waiting for an event
void wakeup_thread(php_spotify_session *session) {
	assert(session != NULL);

	//DEBUG_PRINT("wakeup_thread\n");

	pthread_mutex_lock   (&session->mutex);
	wakeup_thread_lock   (session);
	pthread_mutex_unlock (&session->mutex);
}

// Returns 1 if logged in, 0 otherwise
// This is needed as libspotify will segfault in places if we are no longer logged in!
// Must be called with mutex held
int check_logged_in(php_spotify_session *session) {
	sp_connectionstate  state = sp_session_connectionstate(session->session);
	if (state == SP_CONNECTION_STATE_LOGGED_IN) {
		return 1;
	}
	return 0;
}

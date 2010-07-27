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


// Returns 1 if logged in, 0 otherwise
// This is needed as libspotify will segfault in places if we are no longer logged in!
// Must be called with session mutex held
int session_logged_in(php_spotify_session *session) {
	sp_connectionstate state;

	assert(session != NULL);

	state = sp_session_connectionstate(session->session);
	if (state == SP_CONNECTION_STATE_LOGGED_IN)
		return 1;

	return 0;
}

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

static void tracks_added(sp_playlist *pl, sp_track *const *tracks, int num_tracks, int position, void *userdata) {
	//DEBUG_PRINT("tracks_added\n");
}
static void tracks_removed(sp_playlist *pl, const int *tracks, int num_tracks, void *userdata) {
	//DEBUG_PRINT("tracks_removed\n");
}
static void tracks_moved(sp_playlist *pl, const int *tracks, int num_tracks, int new_position, void *userdata) {
	//DEBUG_PRINT("tracks_moved\n");
}
static void playlist_renamed(sp_playlist *pl, void *userdata) {
	//DEBUG_PRINT("playlist_renamed\n");
}
static void playlist_state_changed(sp_playlist *pl, void *userdata) {
	php_spotify_playlist *playlist = userdata;
	assert(playlist != NULL);

	//DEBUG_PRINT("playlist_state_changed\n");

	wakeup_thread(playlist->session);
}
static void playlist_update_in_progress(sp_playlist *pl, bool done, void *userdata) {
	php_spotify_playlist *playlist = userdata;
	assert(playlist != NULL);

	//DEBUG_PRINT("playlist_update_in_progress (%d)\n", done);

	if (done)
		wakeup_thread(playlist->session);
}
static void playlist_metadata_updated(sp_playlist *pl, void *userdata) {
	php_spotify_playlist *playlist = userdata;
	assert(playlist != NULL);

	//DEBUG_PRINT("playlist_metadata_updated\n");

	wakeup_thread(playlist->session);
}

static sp_playlist_callbacks callbacks = {
	&tracks_added,
	&tracks_removed,
	&tracks_moved,
	&playlist_renamed,
	&playlist_state_changed,
	&playlist_update_in_progress,
	&playlist_metadata_updated,
};

// session->mutex must be locked
static int wait_for_playlist_pending_changes(php_spotify_playlist *playlist) {
	struct timespec ts;
	int err = 0;
	php_spotify_session *session;

	assert(playlist != NULL);
	session = playlist->session;

	//DEBUG_PRINT("wait_for_playlist_pending_changes start\n");

	// Block for a max of 10 seconds
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += SPOTIFY_TIMEOUT;

	while(err == 0) {
		//DEBUG_PRINT("wait_for_playlist_pending_changes loop\n");

		// We first check if we need to process any events
		check_process_events(session);

		if (!sp_playlist_has_pending_changes(playlist->playlist))
			break;

		// Wait until a callback is fired
		err = pthread_cond_timedwait(&session->cv, &session->mutex, &ts);
	}

	//DEBUG_PRINT("wait_for_playlist_pending_changes end(%d)\n", err);
	return err;
}

// session->mutex must be locked
static int wait_for_playlist_loaded(php_spotify_playlist *playlist) {
	struct timespec ts;
	int err = 0;
	php_spotify_session *session;

	assert(playlist != NULL);
	session = playlist->session;

	//DEBUG_PRINT("wait_for_playlist_loaded start\n");

	// Block for a max of 10 seconds
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += SPOTIFY_TIMEOUT;

	while(err == 0) {
		//DEBUG_PRINT("wait_for_playlist_loaded loop\n");

		// We first check if we need to process any events
		check_process_events(session);

		if (sp_playlist_is_loaded(playlist->playlist))
			break;

		// Wait until a callback is fired
		err = pthread_cond_timedwait(&session->cv, &session->mutex, &ts);
	}

	//DEBUG_PRINT("wait_for_playlist_loaded end(%d)\n", err);
	return err;
}


// Creates a playlist resource object
static php_spotify_playlist * playlist_resource_create(php_spotify_session *session, sp_playlist *playlist) {

	int err;
	php_spotify_playlist * resource = emalloc(sizeof(php_spotify_playlist));

	resource->session  = session;
	resource->playlist = playlist;

	sp_playlist_add_callbacks(playlist, &callbacks, resource);

	return resource;
}

static void playlist_resource_destory(php_spotify_playlist *resource) {
	assert(resource != NULL);

	efree(resource);
}

void php_spotify_playlist_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    php_spotify_playlist *playlist = (php_spotify_playlist*)rsrc->ptr;

    if (playlist) {
        playlist_resource_destory(playlist);
    }
}

/* {{{ proto resource spotify_playlist_create(resource session, string name)
   Creates a new playlist with the specified name. Returns false on error. */
PHP_FUNCTION(spotify_playlist_create) {
    sp_playlistcontainer *container;
    sp_playlist *playlist;

    php_spotify_session *session;
    php_spotify_playlist *resource;
    zval *zsession;

	char *name;
	int name_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zsession, &name, &name_len ) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify session (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(session, php_spotify_session*, &zsession, -1, PHP_SPOTIFY_SESSION_RES_NAME, le_spotify_session);

	if (name_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "A blank playlist name was given");
		RETURN_FALSE;
	}

	pthread_mutex_lock(&session->mutex);

    // Always check if some events need processing
    check_process_events(session);

	container = sp_session_playlistcontainer (session->session);
	if (container == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Session is not connected and logged in.");
		goto error;
	}

	playlist = sp_playlistcontainer_add_new_playlist(container, name);
	if (playlist == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to create playlist.");
		goto error;
	}

	resource = playlist_resource_create(session, playlist);
	if (resource == NULL) {
		// An appropriate error will be printed in playlist_resource_create
		goto error;
	}

	wait_for_playlist_pending_changes(resource);

	pthread_mutex_unlock(&session->mutex);

	ZEND_REGISTER_RESOURCE(return_value, resource, le_spotify_playlist);
	return;

error:
	pthread_mutex_unlock(&session->mutex);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string spotify_playlist_name(resource playlist)
   Returns the name of the playlist */
PHP_FUNCTION(spotify_playlist_name) {
    php_spotify_playlist *playlist;
    php_spotify_session  *session;
    zval *zplaylist;
    const char *name;
    int err;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zplaylist ) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify playlist (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(playlist, php_spotify_playlist*, &zplaylist, -1, PHP_SPOTIFY_PLAYLIST_RES_NAME, le_spotify_playlist);

    session = playlist->session;
    pthread_mutex_lock(&session->mutex);

    // Always check if the playlist is loaded
    err = wait_for_playlist_loaded(playlist);
    if (err == ETIMEDOUT) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Timeout while loading playlist metadata.");
		pthread_mutex_unlock(&session->mutex);
		RETURN_FALSE;
    }

    // TODO WAIT FOR PLAYLIST IS LOADED
    name = sp_playlist_name(playlist->playlist);

    pthread_mutex_unlock(&session->mutex);

    RETURN_STRING(name, 1);
}
/* }}} */

/* {{{ proto bool spotify_playlist_rename(resource playlist, string newname)
   Changes the name of the playlist. Returns true on success or false on error. */
PHP_FUNCTION(spotify_playlist_rename) {
    php_spotify_playlist *playlist;
    php_spotify_session  *session;
    zval *zplaylist;
    sp_error err;

    char *name;
	int name_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zplaylist, &name, &name_len ) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify playlist (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(playlist, php_spotify_playlist*, &zplaylist, -1, PHP_SPOTIFY_PLAYLIST_RES_NAME, le_spotify_playlist);

	if (name_len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "A blank playlist name was given");
		RETURN_FALSE;
	}

    session = playlist->session;
    pthread_mutex_lock(&session->mutex);

    // Always check if some events need processing
    check_process_events(session);

	err = sp_playlist_rename(playlist->playlist, name);
	pthread_mutex_unlock(&session->mutex);

	if (err != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = err;
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string spotify_playlist_add_tracks(resource playlist, array tracks)
   Adds the array of spotify track links to this playlist */
PHP_FUNCTION(spotify_playlist_add_tracks) {
    php_spotify_playlist *playlist;
    php_spotify_session  *session;
    zval *zplaylist;

    HashTable *tracks;
    HashPosition tracks_p;  // Used to loop the tracks array

    zval *ztracks;          // The tracks array
    zval **ztrack;          // Each zval in the tracks array

    sp_track **sp_tracks = NULL; // List of tracks passed to libspotify

    int tracks_count;
    int i = 0;
    int return_true = 0;

    sp_error err;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &zplaylist, &ztracks ) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify playlist (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(playlist, php_spotify_playlist*, &zplaylist, -1, PHP_SPOTIFY_PLAYLIST_RES_NAME, le_spotify_playlist);

    tracks = Z_ARRVAL_P(ztracks);
    tracks_count = zend_hash_num_elements(tracks);

    sp_tracks = emalloc(sizeof(sp_track *) * tracks_count);

    session = playlist->session;
    pthread_mutex_lock(&session->mutex);

    // Always check if some events need processing
    check_process_events(session);

    zend_hash_internal_pointer_reset_ex(tracks, &tracks_p);
    while (zend_hash_get_current_data_ex(tracks, (void**) &ztrack, &tracks_p) == SUCCESS) {
    	sp_link *track_link;
    	const char *track_str;

    	// Check the element is a string
    	if (Z_TYPE_PP(ztrack) != IS_STRING) {
    		php_error_docref(NULL TSRMLS_CC, E_WARNING, "All tracks must be valid spotify track links strings");
    		goto cleanup;
    	}

    	track_str = Z_STRVAL_PP(ztrack);

    	// Now try and create a spotify link
    	track_link = sp_link_create_from_string(track_str);
    	if (track_link == NULL) {
    		php_error_docref(NULL TSRMLS_CC, E_WARNING, "\"%s\" is not a valid spotify track link", track_str);
    		goto cleanup;
    	}

    	sp_tracks[i] = sp_link_as_track(track_link);

    	if (sp_tracks[i] == NULL) {
    		php_error_docref(NULL TSRMLS_CC, E_WARNING, "\"%s\" is not a spotify track link", track_str);
    		sp_link_release(track_link);
    		goto cleanup;
    	}

    	// Add a reference to the track, and then free the link
    	sp_track_add_ref(sp_tracks[i]);
    	sp_link_release(track_link);

    	i++;
    	zend_hash_move_forward_ex(tracks, &tracks_p);
    }

    assert(tracks_count == i);

    // Insert all the tracks at the beginning of the playlist
#if SPOTIFY_API_VERSION < 4
    err = sp_playlist_add_tracks(playlist->playlist, (const sp_track **) sp_tracks, tracks_count, 0);
#else
    err = sp_playlist_add_tracks(playlist->playlist, (const sp_track **) sp_tracks, tracks_count, 0, playlist->session->session);
#endif

	if (err != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = err;
		goto cleanup;
	}

	// For this function check that the playlist has actually been commited
	wait_for_playlist_pending_changes(playlist);

	return_true = 1;

cleanup:

	// Release the references
	while(i > 0) {
		i--;
		sp_track_release(sp_tracks[i]);
	}

	pthread_mutex_unlock(&session->mutex);

	efree(sp_tracks);

	if (return_true)
		RETURN_TRUE;

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string spotify_playlist_uri(resource playlist)
   Returns the spotify link for this playlist */
PHP_FUNCTION(spotify_playlist_uri) {
    php_spotify_playlist *playlist;
    php_spotify_session  *session;
    zval *zplaylist;
    sp_link *link;
    char *link_uri;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zplaylist ) == FAILURE) {
        RETURN_FALSE;
    }

    // Check its a spotify playlist (otherwise RETURN_FALSE)
    ZEND_FETCH_RESOURCE(playlist, php_spotify_playlist*, &zplaylist, -1, PHP_SPOTIFY_PLAYLIST_RES_NAME, le_spotify_playlist);

    session = playlist->session;
    pthread_mutex_lock(&session->mutex);

    // Always check if some events need processing
    check_process_events(session);

    link = sp_link_create_from_playlist(playlist->playlist);
    if (link == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "There was a error creating the link");
		pthread_mutex_unlock(&session->mutex);
		RETURN_FALSE;
    }

    link_uri = emalloc(256);

    sp_link_as_string(link, link_uri, 256);
    sp_link_release(link);

    pthread_mutex_unlock(&session->mutex);

    RETURN_STRING(link_uri, 0);
}
/* }}} */


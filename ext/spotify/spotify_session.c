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

/**
 * Session callbacks
 */

static void logged_in          (sp_session *session, sp_error error);
static void logged_out         (sp_session *session);
static void metadata_updated   (sp_session *session);
static void connection_error   (sp_session *session, sp_error error);
static void message_to_user    (sp_session *session, const char *message);
static void notify_main_thread (sp_session *session);
static int  music_delivery     (sp_session *session, const sp_audioformat *format, const void *frames, int num_frames);
static void play_token_lost    (sp_session *session);
static void log_message        (sp_session *session, const char *data);
static void end_of_track       (sp_session *session);

static sp_session_callbacks callbacks = {
	&logged_in,
	&logged_out,
	&metadata_updated,
	&connection_error,
	&message_to_user,
	&notify_main_thread,
	NULL, //&music_delivery,
	NULL, //&play_token_lost,
	&log_message,
	NULL, //&end_of_track,
};

static void logged_in (sp_session *session, sp_error error) {
	php_printf("logged_in\n");
}

static void logged_out (sp_session *session) {
	php_printf("logged_out\n");
}

static void metadata_updated (sp_session *session) {
	php_printf("metadata_updated\n");
}

static void connection_error (sp_session *session, sp_error error) {
	php_printf("connection_error\n");
}

static void message_to_user (sp_session *session, const char *message) {
	php_printf("message_to_user(%s)\n", message);
}

static void notify_main_thread (sp_session *session) {
	php_printf("notify_main_thread\n");
}

static void log_message (sp_session *session, const char *data) {
	php_printf("log_message(%s)\n", data);
}


//sp_session * g_session = NULL;

PHP_FUNCTION(spotify_session_login) {
	sp_session_config config;
	sp_session *session;

	sp_error error;

	char *user;
	char *pass;
	char *key;

	int user_len;
	int pass_len;
	int key_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss",
		&user, &user_len,
		&pass, &pass_len,
		&key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}

	config.api_version = SPOTIFY_API_VERSION;
	config.cache_location    = "tmp"; // TODO change these to be based on the user
	config.settings_location = "tmp";

	config.application_key = key;
	config.application_key_size = key_len;

	config.user_agent = "libspotify for PHP";
	config.callbacks = &callbacks;

	error = sp_session_init(&config, &session);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		RETURN_FALSE;
	}

	error = sp_session_login(session, user, pass);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		// TODO In the future libspotify will add sp_session_release. Call that here.
		RETURN_FALSE;
	}

	// Wait for login?

	RETURN_TRUE;
}

PHP_FUNCTION(spotify_session_logout) {

	sp_session *session;

	sp_error error = sp_session_logout(session);
	if (error != SP_ERROR_OK) {
		SPOTIFY_G(last_error) = error;
		RETURN_FALSE;
	}

	// Wait for logout

	// TODO In the future libspotify will add sp_session_release. Call that here.

	RETURN_TRUE;
}

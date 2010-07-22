<?php
/*
 * Config stuff
 */
	$appkey   = file_get_contents('spotify_appkey.key');
	$username = 'bramp';
	$password = 'testpassword';

/*
 * Some helper code
 */
	function die_error($function) {
		$err = spotify_last_error();
		die("$function failed with error ($err) " . spotify_error_message($err) . "\n");
	}

	$session_states = array(
		SPOTIFY_CONNECTION_STATE_LOGGED_OUT   => 'Logged Out',
		SPOTIFY_CONNECTION_STATE_LOGGED_IN    => 'Logged In',
		SPOTIFY_CONNECTION_STATE_DISCONNECTED => 'Disconnected',
		SPOTIFY_CONNECTION_STATE_UNDEFINED    => 'Undefined'
	);


/*
 * Example of how to connect and do stuff
 */
	$session = spotify_session_login($username, $password, $appkey);
	if (!$session) {
		die_error('spotify_session_login');
	}

	var_dump ( $session );
	echo 'Session state: ' . $session_states[ spotify_session_connectionstate( $session ) ] . "\n";
	echo 'Session user: ' . spotify_session_user( $session ) . "\n";

	spotify_session_logout($session);
	var_dump ( $session );
	exit();

	// Now create a playlist
	$playlist = spotify_playlist_create($session, 'Test');
	if (!$playlist) {
		die_error('spotify_playlist_create');
	}

	var_dump ( $playlist );
	echo 'Playlist name: ' . spotify_playlist_name( $playlist ) . "\n";
	echo 'Playlist link: ' . spotify_playlist_uri ( $playlist ) . "\n";

	$err = spotify_playlist_rename($playlist, 'Test 2');
	if (!$err) {
		die_error('spotify_playlist_create');
	}

	echo 'Playlist new name: ' . spotify_playlist_name( $playlist ) . "\n";
	echo 'Playlist link: '     . spotify_playlist_uri ( $playlist ) . "\n";

	// Now add some tracks
	$tracks = array(
		'spotify:track:4juq76lIar8YWOLpYMXUAG',
		'spotify:track:77J6Ag9kIvxtmPbBt0lFA6',
		'spotify:track:2ihThLuhMTgKnd6yJIj9EW',
		'spotify:track:7q4VZui4uWSHjwdP00SYdg',
		'spotify:track:2yG5Oe2UNDD53cwSicIVGJ'
	);

	$err = spotify_playlist_add_tracks($playlist, $tracks);
	if (!$err) {
		die_error('spotify_playlist_add_tracks');
	}

	spotify_session_logout($session);
?>

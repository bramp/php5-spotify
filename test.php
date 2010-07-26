<pre>
<?php
/*
 * Config stuff
 */
	$appkey   = @file_get_contents('spotify_appkey.key');
	$username = 'bramp';
	$password = 'testpassword';

	if ($appkey === false) {
		die('Please download a binary libspotify appkey from developer.spotify.com and save it as spotify_appkey.key in the current directory\n');
	}

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

	//exit();

	// Use the time as the playlist name (just for debuggin purposes)
	$playlistName = date("H:i:s");

	// Now create a playlist
	$playlist = spotify_playlist_create($session, $playlistName . '_before');
	if (!$playlist) {
		die_error('spotify_playlist_create');
	}

	var_dump ( $playlist );
	echo 'Playlist name: ' . spotify_playlist_name( $playlist ) . "\n";
	echo 'Playlist link: ' . spotify_playlist_uri ( $playlist ) . "\n";

	// Now rename it (for the sake of it)
	$err = spotify_playlist_rename($playlist, $playlistName . '_after');
	if (!$err) {
		die_error('spotify_playlist_create');
	}

	echo 'Playlist new name: ' . spotify_playlist_name( $playlist ) . "\n";
	echo 'Playlist link: '     . spotify_playlist_uri ( $playlist ) . "\n";

	// Now add some tracks
	$tracks = array (
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

	// I absolutely hate this BUT, if we don't sleep here the changes don't get sent back
	// to the spotify server. Inside the spotify_playlist_add_tracks call I check if the
	// changes have been saved, before returning. Libspotify says they are saved, but obviously
	// not.
	sleep(10);

?>

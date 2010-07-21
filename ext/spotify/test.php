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
?>

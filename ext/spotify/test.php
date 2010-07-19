<?php
	$appkey   = file_get_contents('spotify_appkey.key');
	$username = 'bramp';
	$password = 'testpassword';

	function die_error($function) {
		$err = spotify_last_error();
		die("$function failed with error ($err) " . spotify_error_message($err) . "\n");
	}

	$session = spotify_session_login($username, $password, $appkey);
	if (!$session) {
		die_error('spotify_session_login');
	}

	echo 'Session user: ' . spotify_session_user( $session ) . "\n";

	var_dump ( $session )
?>

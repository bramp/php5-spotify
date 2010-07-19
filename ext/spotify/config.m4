PHP_ARG_ENABLE(spotify, whether to enable Spotify support,
[ --enable-spotify   Enable Spotify support])

if test "$PHP_SPOTIFY" = "yes"; then
  AC_DEFINE(HAVE_SPOTIFY, 1, [Whether you have Spotify])
  PHP_SUBST(SPOTIFY_SHARED_LIBADD)
  PHP_ADD_LIBRARY(spotify, 1, SPOTIFY_SHARED_LIBADD)
  PHP_NEW_EXTENSION(spotify, spotify.c spotify_session.c spotify_error.c spotify_playlist.c, $ext_shared)
fi

dnl $Id$
dnl config.m4 for extension spotify

PHP_ARG_WITH(spotify, for spotify support,
[  --with-spotify             Include spotify support])

if test "$PHP_SPOTIFY" != "no"; then
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="/include/libspotify/api.h"
  if test -r $PHP_SPOTIFY/$SEARCH_FOR; then # path given as parameter
    SPOTIFY_DIR=$PHP_SPOTIFY
  else # search default path list
    AC_MSG_CHECKING([for spotify files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        SPOTIFY_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi

  if test -z "$SPOTIFY_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the spotify distribution])
  fi

  PHP_ADD_INCLUDE($SPOTIFY_DIR/include)

  LIBNAME=spotify
  LIBSYMBOL=sp_session_init

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $SPOTIFY_DIR/lib, SPOTIFY_SHARED_LIBADD)
    AC_DEFINE(HAVE_SPOTIFYLIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong spotify lib version or lib not found])
  ],[
    -L$SPOTIFY_DIR/lib -lm
  ])

  PHP_SUBST(SPOTIFY_SHARED_LIBADD)

  PHP_NEW_EXTENSION(spotify, spotify.c spotify_error.c spotify_playlist.c spotify_session.c, $ext_shared)
fi

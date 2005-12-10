AC_PREREQ(2.50)

AC_INIT(util/util.h)
AM_INIT_AUTOMAKE(jadc2s,0.9.1-RC1)
AM_CONFIG_HEADER(config.h)

dnl helper macros
sinclude(ac-helpers/ac_define_dir.m4)

dnl for developers
AC_MSG_CHECKING(if developer mode enabled)
AC_ARG_ENABLE(developer, AC_HELP_STRING([--enable-developer],
                    [Developer mode]),
              developer=yes)

if test x-$developer = "x-yes" ; then
    AC_MSG_RESULT(yes)
    CFLAGS="$CFLAGS -Wall -g"
else
    AC_MSG_RESULT(no)
fi


dnl Check for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_ICONV

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl use libtool to compile checks (particularly lib checks), mostly so we don't have to worry about how/if the os supports -R
ac_link="${SHELL} ${srcdir}/libtool --mode=link $ac_link"

dnl headers we need
AC_HEADER_STDC

dnl static builds
AC_MSG_CHECKING(if static builds enabled)
AC_ARG_ENABLE(all-static, AC_HELP_STRING([--enable-all-static], [Build static binaries]), all_static=yes, all_static=no)
if test "x-$all_static" = "x-yes" ; then
    LDFLAGS="$LDFLAGS -Wl,-static -static"
fi
AC_MSG_RESULT($all_static)

AC_MSG_CHECKING(if partial static builds enabled)
AC_ARG_ENABLE(partial-static, AC_HELP_STRING([--enable-partial-static], [Build partially static binaries]), partial_static=yes, partial_static=no)
if test "x-$partial_static" = "x-yes" ; then
    LDFLAGS="$LDFLAGS -Wl,-lc,-static -static"
fi
AC_MSG_RESULT($partial_static)

dnl solaris has socket functions in libsocket
AC_CHECK_FUNC(socket, have_libsocket=yes, have_libsocket=no)
if test "$have_libsocket" = "yes"; then
    AC_DEFINE(HAVE_LIBSOCKET, 1, [Define if socket and associated functions are available.])
else
    AC_CHECK_LIB(socket, socket, have_libsocket=yes, have_libsocket=no)
    if test "$have_libsocket" = "yes"; then
        AC_DEFINE(HAVE_LIBSOCKET, 1, [Define if socket and associated functions are available.])
        LIBS="$LIBS -lsocket"
    fi
fi
if test "$have_libsocket" = "no" ; then
    AC_MSG_ERROR([Couldn't find required function socket]);
fi

dnl check for needed functions to enable IPv6

AC_MSG_CHECKING(for inet_pton)
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>], [inet_pton(0, (char *)0, (char *)0);], has_inet_pton=yes, has_inet_pton=no)
if test "$has_inet_pton" = yes; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(for inet_ntop)
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>], [inet_ntop(0, (char *)0, (char *)0, 0);], has_inet_ntop=yes, has_inet_ntop=no)
if test "$has_inet_ntop" = yes; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(for getaddrinfo)
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>], [getaddrinfo(NULL, NULL, NULL, NULL)], has_getaddrinfo=yes, has_getaddrinfo=no)
if test "$has_getaddrinfo" = yes; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(if IPv6 is requested)
AC_ARG_ENABLE(ipv6, AC_HELP_STRING([--enable-ipv6], [Enable IPv6 code]), ipv6=yes)
if test x-$has_inet_pton = "x-no" ; then
    AC_MSG_RESULT(disabled: inet_pton missing)
else if test x-$has_inet_ntop = "x-no" ; then
    AC_MSG_RESULT(disabled: inet_ntop missing)
else if test x-$has_getaddrinfo = "x-no" ; then
    AC_MSG_RESULT(disabled: getaddrinfo missing)
else if test x-$ipv6 = "x-yes" ; then
    AC_MSG_RESULT(enabled)
    AC_DEFINE(USE_IPV6,,[IPv6 code enabled])
else
    AC_MSG_RESULT(disabled)
fi
fi
fi
fi

AC_SUBST(USE_IPV6)

dnl ** Check for broken glibc with __ss_family
                                                                                                                             
AC_MSG_CHECKING(for broken glibc with __ss_family)
AC_TRY_COMPILE([#include <sys/socket.h>], [struct sockaddr_storage s; s.__ss_family = AF_INET;], have_broken_ss_family=yes, have_broken_ss_family=no)
if test "$have_broken_ss_family" = yes; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(ss_family,__ss_family,[use __ss_family instead of ss_family])
else
    AC_MSG_RESULT(no)
fi

dnl ** Check for broken glibc with __ss_len

AC_MSG_CHECKING(for broken glibc with __ss_len)
AC_TRY_COMPILE([#include <sys/socket.h>], [struct sockaddr_storage s; s.__ss_len = 0;], have_broken_ss_len=yes, have_broken_ss_len=no)
if test "$have_broken_ss_len" = yes; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(ss_len,__ss_len,[use __ss_len instead of ss_len])
else
    AC_MSG_RESULT(no)
fi

dnl MIO backend
mio_backend="poll"
AC_MSG_CHECKING(which mio backend to use)
AC_ARG_ENABLE(mio, AC_HELP_STRING([--enable-mio=BACKEND], [Select the mio backend (default: poll)]), 
            mio_backend=$enableval)

case x-$mio_backend in
    x-poll)
        AC_MSG_RESULT(poll)
        AC_DEFINE(MIO_POLL,,[MIO poll backend]);;
    x-select)
        AC_MSG_RESULT(select)
        AC_DEFINE(MIO_SELECT,,[MIO select backend]);;
    *)
        AC_MSG_ERROR([Unknown MIO backend: $mio_backend]);;
esac;


dnl debugging
AC_MSG_CHECKING(if debug messages wanted)
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
                    [Enable debug messages]),
              debug=yes)

if test x-$debug = "x-yes" ; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(DEBUG,,[app debug enabled])
else
    AC_MSG_RESULT(no)
fi

AC_SUBST(DEBUG)

AC_MSG_CHECKING(if pool debugging wanted)
AC_ARG_ENABLE(pool_debug, AC_HELP_STRING([--enable-pool-debug], [Enable pool debugging]), pool_debug=yes)
if test x-$pool_debug = "x-yes" ; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(POOL_DEBUG,,[pool debug enabled])
else
    AC_MSG_RESULT(no)
fi

AC_SUBST(POOL_DEBUG)

dnl check where to log
AC_MSG_CHECKING(if logging to syslog wanted)
AC_ARG_ENABLE(syslog, AC_HELP_STRING([--enable-syslog], [Enable logging to syslog]), syslog=yes)
if test x-$syslog = "x-yes" ; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(USE_SYSLOG,,[logging to syslog enabled])
else
    AC_MSG_RESULT(no)
fi

AC_SUBST(USE_SYSLOG)


dnl check if SSL/TLS is requested
PKG_CHECK_MODULES(OPENSSL, openssl >= 0.9.7, hasopenssl=yes, hasopenssl=no)
if test $hasopenssl = "no" ; then
    AC_MSG_RESULT($OPENSSL_PKG_ERRORS)
fi
AC_MSG_CHECKING(if OpenSSL is requsted)
AC_ARG_ENABLE(ssl, AC_HELP_STRING([--enable-ssl], [Enable protection with SSL/TLS]), ssl=yes)
if test x-$ssl = "x-yes" ; then
    AC_MSG_RESULT(yes)
    if test $hasopenssl = "yes" ; then
	AC_DEFINE(USE_SSL,,[SSL/TLS with openssl requested])
	CFLAGS="$CFLAGS $OPENSSL_CFLAGS"
	LDFLAGS="$LDFLAGS $OPENSSL_LIBS"
    else
	AC_MSG_ERROR([SSL is requested but usable OpenSSL installation has not been found.])
    fi
else
    AC_MSG_RESULT(no)
fi

AC_SUBST(USE_SSL)

dnl check for libgsasl
PKG_CHECK_MODULES(GSASL, libgsasl >= 0.2.0, haslibgsasl=yes, haslibgsasl=no)
if test $haslibgsasl = "no" ; then
    AC_MSG_RESULT($GSASL_PKG_ERRORS)
fi
AC_MSG_CHECKING(if SASL is requested)
AC_ARG_ENABLE(sasl, AC_HELP_STRING([--enable-sasl], [Enable SASL authentication using GNU SASL]), sasl=yes)
if test x-$sasl = "x-yes" ; then
    AC_MSG_RESULT(yes)
    if test $haslibgsasl = "yes" ; then
	AC_DEFINE(WITH_SASL,,[GNU SASL requested])
	CFLAGS="$CFLAGS $GSASL_CFLAGS"
	LDFLAGS="$LDFLAGS $GSASL_LIBS"
    else
	AC_MSG_ERROR([SASL is requested but usable GNU SASL installation has not been found.])
    fi
else
    AC_MSG_RESULT(no)
fi

AC_SUBST(WITH_SASL)

dnl check if flash hack is requested
AC_MSG_CHECKING(if flash hack wanted)
AC_ARG_ENABLE(flash, AC_HELP_STRING([--enable-flash],
                    [Enable a hack that supports flash clients]),
              flash=yes)

if test x-$flash = "x-yes" ; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(FLASH_HACK,,[flash hack requested])
else
    AC_MSG_RESULT(no)
fi

dnl check for expat
AC_ARG_WITH(libexpat, AC_HELP_STRING([--with-expat=DIR],
            [Where to find libexpat (required)]),
            libexpat=$withval, libexpat=yes)
if test "$libexpat" != "no"; then
    if test "$libexpat" != "yes"; then
        LDFLAGS="${LDFLAGS} -L$libexpat/lib"
        CPPFLAGS="${CPPFLAGS} -I$libexpat/include"
    fi
    AC_CHECK_HEADER(expat.h,
                    AC_CHECK_LIB(expat, XML_ParserCreate,
                                 [libexpat=yes LIBS="${LIBS} -lexpat"], libexpat=no),
                                 libexpat=no)
fi
if test "$libexpat" != "yes"; then
    AC_MSG_ERROR([Couldn't find required libexpat installation]);
fi

dnl check for libidn
AC_ARG_WITH(libidn, AC_HELP_STRING([--with-libidn=DIR],
	    [Support IDN (needs GNU Libidn)]),
	    libidn=$withval, libidn=yes)
if test "$libidn" != "no"; then
    if test "$libidn" != "yes"; then
	LDFLAGS="${LDFLAGS} -L$libidn/lib"
	CPPFLAGS="${CPPFLAGS} -I$libidn/include"
    fi
    AC_CHECK_HEADER(idna.h,
		    AC_CHECK_LIB(idn, stringprep_check_version,
				 [libidn=yes LIBS="${LIBS} -lidn"], libidn=no),
				 libidn=no)
fi
if test "$libidn" != "no"; then
    AC_DEFINE(LIBIDN, 1, [Define to 1 if you want IDN support.])
else
    AC_MSG_ERROR([Could not find required Libidn installation])
fi
AC_MSG_CHECKING([if Libidn should be used])
AC_MSG_RESULT($libidn)


dnl define where the configuration file is located
AC_DEFINE_DIR(CONFIG_DIR,sysconfdir,[where the configuration file can be found])

dnl Create the makefiles
AC_OUTPUT(Makefile \
	  mio/Makefile \
	  util/Makefile \
	  ac-helpers/Makefile)
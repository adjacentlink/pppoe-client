AC_COPYRIGHT([

Copyright (c) 2013 - 2014 - Adjacent Link, LLC, Bridgewater NJ 
All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

See individual files for specific copyright information.
])

AC_INIT([pppoe-client],0.2.1,[labs at adjacent link dot com])

AC_CONFIG_SRCDIR([src])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([subdir-objects])

AC_PROG_CXX

AC_PROG_CXXCPP

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

AC_PROG_INSTALL

AC_PROG_LIBTOOL

AC_LANG([C++])

AC_ARG_WITH(profile,
           [  --with-profile          add profiling support (off)])

AC_ARG_WITH(debug,
           [  --with-debug            add debug support (off)])

AC_ARG_VAR([libemane_CFLAGS],[C compiler flags for EMANE])
AC_ARG_VAR([libemane_LIBS], [linker flags for EMANE])

PKG_CHECK_MODULES([libxml2], libxml-2.0)
PKG_CHECK_MODULES([protobuf], protobuf)
PKG_CHECK_MODULES([libpcre], libpcre)
PKG_CHECK_MODULES([libuuid], uuid)

# compiler options
CFLAGS="$CFLAGS -W -Wall -g3 -O0 -Wformat -std=c99 -D_GNU_SOURCE"
CXXFLAGS="$CXXFLAGS -W -Wall -g3 -O0 -Wformat -D_GNU_SOURCE"

# Checks for libraries.
# Saving original cpp and ld flags
_cppflags=${CPPFLAGS}
_ldflags=${LDFLAGS}

# Reverting to original cpp and ld flags
CPPFLAGS=${_cppflags}
LDFLAGS=${_ldflags}

# libemane check
if test "${libemane_CFLAGS}" == ""; then
   libemane_CFLAGS="${libxml2_CFLAGS} ${protobuf_CFLAGS} ${libpcre_CFLAGS} ${libuuid_CFLAGS}"
fi

CPPFLAGS="${libemane_CFLAGS} ${CPPFLAGS} -std=c++11"


if test "${libemane_LIBS}" == ""; then
   libemane_LIBS="-lemane -lrt ${libxml2_LIBS} ${protobuf_LIBS} ${libpcre_LIBS} ${libuuid_LIBS}"
fi

LDFLAGS="${libemane_LIBS} ${LDFLAGS}"

AC_CHECK_HEADER(emane/application/transportbuilder.h,
 [],
 [AC_MSG_ERROR(["EMANE lib not installed"])])

# Reverting to original cpp and ld flags
CPPFLAGS=${_cppflags}
LDFLAGS=${_ldflags}

AC_C_BIGENDIAN(AC_DEFINE([__BYTE_ORDER],__BIG_ENDIAN,[Big Endian]),AC_DEFINE([__BYTE_ORDER],__LITTLE_ENDIAN,[Little Endian]))
AC_DEFINE([__LITTLE_ENDIAN],1234,[for the places where it is not defined])
AC_DEFINE([__BIG_ENDIAN],4321,[for the places where it is not defined])

# options for use with gprof
if test "$with_profile" = "yes"
then
CPPFLAGS="$CPPFLAGS -g -pg"
LDFLAGS="$LDFLAGS -g -pg"
fi

# options for use with debug
if test "$with_debug" = "yes"
then
CPPFLAGS="$CPPFLAGS -g -O0 -DDEBUG"
LDFLAGS="$LDFLAGS -g -O0"
fi

LANG=C
AC_SUBST(LANG)

RELDATE=`date +'%a %b %e %Y'`
AC_SUBST(RELDATE)

# check for rpmbuild
AC_CHECK_PROG(HAVE_RPMBUILD, rpmbuild, true, false)
AM_CONDITIONAL(HAVE_RPMBUILD,$HAVE_RPMBUILD)

# check for deb
AC_CHECK_PROG(HAVE_DEB, dh_clean, true, false)
AM_CONDITIONAL(HAVE_DEB,$HAVE_DEB)

AC_OUTPUT(
 Makefile 
 src/Makefile
 src/pppoe-client/Makefile
 src/pppoe-client/pppoe/Makefile
 src/pppoe-server/Makefile
 src/pppoe-server/pppoe/Makefile
 src/ctrl/Makefile
 conf/Makefile
 scripts/Makefile
 pppoe-client.spec:pppoe-client.spec.in
)

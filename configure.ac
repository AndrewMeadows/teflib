AC_INIT([teflib], [1.0], [])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Determine if we're doing an out-of-source build
AC_MSG_CHECKING([for out-of-source build])
if test "$srcdir" != "." ; then
    AC_MSG_RESULT([yes (builddir: $ac_pwd)])
else
    AC_MSG_RESULT([no - consider using: mkdir build && cd build && ../configure])
fi

AC_PROG_CXX
AC_PROG_RANLIB
AM_PROG_AR

# Set ARFLAGS to avoid 'u' modifier warning (modern ar uses 'D' by default)
AC_SUBST([ARFLAGS], [cr])

# Check for USE_TEF environment variable
AC_MSG_CHECKING([whether to enable tracing])
if test "x$USE_TEF" != "x"; then
    AC_MSG_RESULT([yes])
    use_tef=true
else
    AC_MSG_RESULT([no])
    use_tef=false
fi
AM_CONDITIONAL([USE_TEF], [test x$use_tef = xtrue])

# Require C++17
CXXFLAGS="$CXXFLAGS -std=c++17"


AC_CONFIG_FILES([
    Makefile
    src/Makefile
    example/Makefile
])

AC_OUTPUT

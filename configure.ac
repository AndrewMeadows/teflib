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

# Require C++14 - handled in Makefile.am files via -std=c++14

# Initialize git submodules if needed
AC_MSG_CHECKING([for git submodules])
if test -d "$srcdir/.git" && test ! -f "$srcdir/external/fmt/CMakeLists.txt"; then
    AC_MSG_RESULT([initializing])
    cd "$srcdir" && git submodule update --init --recursive
else
    AC_MSG_RESULT([already initialized])
fi

# Check for fmt library in submodule
AC_MSG_CHECKING([for fmt library])
if test -f "$srcdir/external/fmt/include/fmt/core.h"; then
    AC_MSG_RESULT([yes])
    FMT_CPPFLAGS="-I\$(top_srcdir)/external/fmt/include"
    FMT_LDFLAGS=""
    FMT_LIBS="\$(top_builddir)/build/external/libfmt.a"
else
    AC_MSG_ERROR([fmt library not found in external/fmt])
fi

AC_SUBST([FMT_CPPFLAGS])
AC_SUBST([FMT_LDFLAGS])
AC_SUBST([FMT_LIBS])

AC_CONFIG_FILES([
    Makefile
    external/Makefile
    src/Makefile
    example/Makefile
])

AC_OUTPUT

# Build fmt library from submodule
noinst_LIBRARIES = libfmt.a

libfmt_a_SOURCES = \
    fmt/src/format.cc \
    fmt/src/os.cc

libfmt_a_CPPFLAGS = -I$(srcdir)/fmt/include -std=c++14

# Copy libfmt.a to build/external after building
all-local:
	$(MKDIR_P) $(top_builddir)/build/external
	cp -f libfmt.a $(top_builddir)/build/external/libfmt.a

clean-local:
	rm -f $(top_builddir)/build/external/libfmt.a

# Install fmt headers (optional, for development)
fmtincludedir = $(includedir)/fmt
nobase_fmtinclude_HEADERS = \
    fmt/include/fmt/core.h \
    fmt/include/fmt/format.h \
    fmt/include/fmt/format-inl.h \
    fmt/include/fmt/os.h \
    fmt/include/fmt/chrono.h \
    fmt/include/fmt/color.h \
    fmt/include/fmt/compile.h \
    fmt/include/fmt/ostream.h \
    fmt/include/fmt/printf.h \
    fmt/include/fmt/ranges.h

noinst_LIBRARIES = libteflib.a

libteflib_a_SOURCES = \
    trace.cpp \
    trace.h

# Unit tests
if ENABLE_UNIT_TESTS
noinst_PROGRAMS = test/trace_test
test_trace_test_SOURCES = test/trace_test.cpp
test_trace_test_CPPFLAGS = -I$(top_srcdir)/external/json/include
test_trace_test_LDADD = libteflib.a
endif

# Install teflib headers
teflibincludedir = $(includedir)/teflib
teflibinclude_HEADERS = trace.h

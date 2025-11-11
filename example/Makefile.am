bin_PROGRAMS = example

example_SOURCES = \
    main.cpp \
    util/thread_pool.h \
    util/timing_util.cpp \
    util/timing_util.h \
    util/log_util.cpp \
    util/log_util.h

example_CPPFLAGS = -I$(top_srcdir)/src
if USE_TEF
example_CPPFLAGS += -DUSE_TEF
endif
example_LDADD = $(top_builddir)/src/libteflib.a

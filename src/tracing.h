// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
// This file contains the macros for using the tracing library.
// Include this file after trace.h to use the tracing macros.

#pragma once

#include "trace.h"

    // We use 'do{}while(0)' as the no-op because:
    // (1) Most compilers will optimize it out
    // (2) It is a single expression and should not unexpectedly break contexts
    //     (e.g. when used in a single-line-'if' context without braces)
    #define TRACE_NOOP do{}while(0)

#ifdef USE_TEF
    #include "consumer.h"

    // Always invoke TRACE_GLOBAL_INIT in main global space.
    // It initializes g_trace_consumer_ptr which points at the glboal Trace_to_file instance
    // when tracing is active, or to nullptr when not.
    #define TRACE_GLOBAL_INIT std::unique_ptr<tef::Trace_to_file> g_trace_consumer_ptr;

    // Register a string for use with TRACE_CONTEXT.
    // The index must be a uint8_t value (0-255).
    #define TRACE_REGISTER_STRING(index, str) ::tef::Trace::instance().register_string(index, str)

    // Start tracing to filename for a duration of lifetime_msec.
    // After lifetime_msec the Trace_to_file instance will be "complete" and will be
    // deleted inside TRACE_MAINLOOP.
    #define TRACE_START(lifetime_msec, filename) \
        g_trace_consumer_ptr = std::make_unique<tef::Trace_to_file>(lifetime_msec, filename); \
        ::tef::Trace::instance().add_consumer(g_trace_consumer_ptr.get());

    // Check if tracing is currently active
    #define TRACE_IS_ACTIVE() (g_trace_consumer_ptr != nullptr)

    // Stop tracing early by setting expiry to 0
    #define TRACE_STOP_EARLY() if (g_trace_consumer_ptr) g_trace_consumer_ptr->update_expiry(0);

    // Get the current trace filename (returns empty string if not tracing)
    #define TRACE_GET_FILENAME() (g_trace_consumer_ptr ? g_trace_consumer_ptr->get_filename() : std::string())

    // Process accumulated trace Events and check expiry of the active trace
    #define TRACE_MAINLOOP ::tef::Trace::instance().advance_consumers(); if(g_trace_consumer_ptr && g_trace_consumer_ptr->is_complete()) g_trace_consumer_ptr.reset();

    // Stop tracing and make sure tracing doesn't leak memory.
    #define TRACE_SHUTDOWN ::tef::Trace::instance().shutdown(); if (g_trace_consumer_ptr) g_trace_consumer_ptr.reset();

    // Invoke TRACE_PROCESS once during init to supply a name for the process.
    // Otherwise the process will be given some arbitrary numerical name.
    #define TRACE_PROCESS(name) ::tef::Trace::instance().add_meta_event("process_name", name);

    // Invoke TRACE_THREAD once per thread to name the thread in the final report.
    // Otherwise the thread will be given a numerical name.
    #define TRACE_THREAD(name) ::tef::Trace::instance().add_meta_event("thread_name", name);

    // Invoke TRACE_THREAD_SORT once per thread to assign a sorting index when the Trace data
    // is displayed in the trace browser. Otherwise the threads will be sorted in arbitrary order.
    #define TRACE_THREAD_SORT(index) ::tef::Trace::instance().add_meta_event("thread_sort_index", index);

    // Create trace Events inside the local context.
    // name = index to the registered context name string
    // categories = index to the registered categories string
    #define TRACE_CONTEXT(name, categories) ::tef::Trace::Context _tef_context_(name, categories);

    // Add an arg that will be added to the Event in the stream when the report is generated.
    #define TRACE_CONTEXT_ARG(name, value)  _tef_context_.add_arg({name, value});

    // Add a Counter
    // TODO: figure out how to support multiple counts
    #define TRACE_COUNTER(name, count_name, count) ::tef::Trace::instance().set_counter(name, count_name, int64_t(count));

    // use TRACE_BEGIN/END when you know what you're doing
    // and when TRACE_CONTEXT does not quite do what you need
    #define TRACE_BEGIN(name, categories) ::tef::Trace::instance().add_event(name, categories, ::tef::Phase::DurationBegin);
    #define TRACE_END(name, categories) ::tef::Trace::instance().add_event(name, categories, ::tef::Phase::DurationEnd);

#else   // USE_TEF
    // When USE_TEF is undefined all macros translate to no-ops.

    #define TRACE_GLOBAL_INIT int _foo_(){return 0;}
    #define TRACE_REGISTER_STRING(index, str) TRACE_NOOP;

    #define TRACE_START(lifetime_msec, filename) TRACE_NOOP;
    #define TRACE_IS_ACTIVE() false
    #define TRACE_STOP_EARLY() TRACE_NOOP;
    #define TRACE_GET_FILENAME() std::string()
    #define TRACE_MAINLOOP TRACE_NOOP;
    #define TRACE_SHUTDOWN TRACE_NOOP;

    #define TRACE_PROCESS(name) TRACE_NOOP;
    #define TRACE_THREAD(name) TRACE_NOOP;
    #define TRACE_THREAD_SORT(index) TRACE_NOOP;

    #define TRACE_CONTEXT(name, categories) TRACE_NOOP;
    #define TRACE_CONTEXT_ARG(fmt, value) TRACE_NOOP;

    #define TRACE_COUNTER(name, count_name, count) TRACE_NOOP;

    #define TRACE_BEGIN(name, categories) TRACE_NOOP;
    #define TRACE_END(name, categories) TRACE_NOOP;

#endif // USE_TEF

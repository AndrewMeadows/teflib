// tracing.h -- macros for using the teflib tracing library.
//
// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

//
// teflib is a C++ utility for generating trace report data as per the Google Trace Event Format (TEF):
//
//     https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit#heading=h.yr4qxyxotyw
//
// A suggested usage pattern is to use the macros at the bottom of this file:
//
// (1) Add trace.h and trace.cpp to your project or link pre-compiled teflib library.
//
// (2) In main global space (before main() or in global namespace) add the macro: TRACE_GLOBAL_INIT
//
// (3) In main entrypoint register strings used to name various contexts and category filters.
//     The string registration allows the tracing macros to avoid expensive allocations at runtime
//     which reduces the overhead of the trace operations.
//
// (4) Put TRACE_END_MAINLOOP at the end of the mainloop
//
// (5) After mainloop completes but before exit: add the macro TRACE_SHUTDOWN
//
// (6) In any context for which you want to measure duration add a macro:
//        TRACE_CONTEXT(name, categories)
//     Declare only once per scope - multiple TRACE_CONTEXT instances in the same scope
//     will cause variable name collisions and compilation errors.
//     name = uint8_t index to pre-registered string name of context
//     categories = uing8_t index to pre-registered string of comma-separated words used for filtering
//
// (7) Compile project with -DUSE_TEF
//
// Example usage:
/*
    #include <cassert>

    #define USE_TEF
    #include "tracing.h"

    // using constexpr for trace string indices isn't required
    // but simplifies multi-threaded context of the indices
    // and minimizes the compiled runtime overhead
    constexpr uint8_t MY_FUNCTION = 0;
    constexpr uint8_t WORK = 1;

    void register_trace_strings() {
        TRACE_REGISTER_STRING(MY_FUNCTION, "my_function");
        TRACE_REGISTER_STRING(WORK, "work");
    }

    void my_function() {
        TRACE_CONTEXT(MY_FUNCTION, WORK);
        size_t sum = 0;
        for (size_t i = 0; i < 2000; ++i)
        {
            sum += i;
        }
    }

    int main() {
        // init trace strings
        register_trace_strings();

        // Start tracing to file for 5 seconds
        TRACE_START(5000, "trace.json");

        size_t num_loops = 0;
        while (num_loops < 1000) {
            my_function();
            ++num_loops;
            TRACE_END_MAINLOOP
        }

        TRACE_SHUTDOWN
        return 0;
    }
*/

#pragma once

#include "trace.h"

    // We use 'do{}while(0)' as the no-op because:
    // (1) Most compilers will optimize it out
    // (2) It is a single expression and should not unexpectedly break contexts
    //     (e.g. when used in a single-line-'if' context without braces)
    #define TRACE_NOOP do{}while(0)

#ifdef USE_TEF
    #include "consumer.h"

    // Register a string for use with TRACE_CONTEXT.
    // The index must be a uint8_t value (0-255).
    #define TRACE_REGISTER_STRING(index, str) ::tef::Trace::instance().register_string(index, str)

    // Start tracing to filename for a duration of lifetime_msec.
    // After lifetime_msec the Default_consumer instance will be "complete" and will be
    // deleted inside TRACE_END_MAINLOOP.
    #define TRACE_START(lifetime_msec, filename) \
        ::tef::Default_consumer::instance().start_trace(lifetime_msec, filename), \
        ::tef::Trace::instance().add_consumer(&tef::Default_consumer::instance())

    // Check if tracing is currently active
    #define TRACE_IS_ACTIVE() ::tef::Default_consumer::instance().is_active()

    // Stop tracing early by setting expiry to 0
    #define TRACE_STOP_EARLY() ::tef::Default_consumer::instance().stop()

    // Get the current trace filename (returns empty string if not tracing)
    #define TRACE_GET_FILENAME() ::tef::Default_consumer::instance().get_filename()

    // Process accumulated trace Events and check expiry of the active trace
    #define TRACE_END_MAINLOOP ::tef::Trace::instance().advance_consumers()

    // Stop tracing and make sure tracing doesn't leak memory.
    #define TRACE_SHUTDOWN ::tef::Trace::instance().shutdown()

    // Invoke TRACE_PROCESS once during init to supply a name for the process.
    // Otherwise the process will be given some arbitrary numerical name.
    #define TRACE_PROCESS(name) ::tef::Trace::instance().add_meta_event("process_name", name)

    // Invoke TRACE_THREAD once per thread to name the thread in the final report.
    // Otherwise the thread will be given a numerical name.
    #define TRACE_THREAD(name) ::tef::Trace::instance().add_meta_event("thread_name", name)

    // Invoke TRACE_THREAD_SORT once per thread to assign a sorting index when the Trace data
    // is displayed in the trace browser. Otherwise the threads will be sorted in arbitrary order.
    #define TRACE_THREAD_SORT(index) ::tef::Trace::instance().add_meta_event("thread_sort_index", index)

    // Create trace Events inside the local context.
    // name = index to the registered context name string
    // categories = index to the registered categories string
    #define TRACE_CONTEXT(name, categories) ::tef::Trace::Context _tef_context_(name, categories)

    // Add an arg that will be added to the Event in the stream when the report is generated.
    #define TRACE_CONTEXT_ARG(name, value)  _tef_context_.add_arg({name, value})

    // Add a Counter
    // TODO: figure out how to support multiple counts
    #define TRACE_COUNTER(name, count_name, count) ::tef::Trace::instance().set_counter(name, count_name, int64_t(count))

    // use TRACE_BEGIN/END when you know what you're doing
    // and when TRACE_CONTEXT does not quite do what you need
    #define TRACE_BEGIN(name, categories) ::tef::Trace::instance().add_event(name, categories, ::tef::Phase::DurationBegin)
    #define TRACE_END(name, categories) ::tef::Trace::instance().add_event(name, categories, ::tef::Phase::DurationEnd)

#else   // USE_TEF
    // When USE_TEF is undefined all macros translate to no-ops.

    #define TRACE_REGISTER_STRING(index, str) TRACE_NOOP

    #define TRACE_START(lifetime_msec, filename) TRACE_NOOP
    #define TRACE_IS_ACTIVE() false
    #define TRACE_STOP_EARLY() TRACE_NOOP;
    #define TRACE_GET_FILENAME() std::string()
    #define TRACE_END_MAINLOOP TRACE_NOOP
    #define TRACE_SHUTDOWN TRACE_NOOP

    #define TRACE_PROCESS(name) TRACE_NOOP
    #define TRACE_THREAD(name) TRACE_NOOP
    #define TRACE_THREAD_SORT(index) TRACE_NOOP

    #define TRACE_CONTEXT(name, categories) TRACE_NOOP
    #define TRACE_CONTEXT_ARG(fmt, value) TRACE_NOOP

    #define TRACE_COUNTER(name, count_name, count) TRACE_NOOP

    #define TRACE_BEGIN(name, categories) TRACE_NOOP
    #define TRACE_END(name, categories) TRACE_NOOP

#endif // USE_TEF

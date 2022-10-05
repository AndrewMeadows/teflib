// teflib/example/main.cpp
//
// This example shows the pattern for using teflib for tracing.
// Typically tracing doesn't happen immediately: it is triggered
// by some runtime event, like a GUI button.  In this simple
// no-GUI demo we do it by handling SIGUSR2.
//
// To trigger a trace from the CLI you might run a bash script like so:
//
// #!bash
// PID=./teflib_example
// kill --SIGUSR2 $PID

#include "trace.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <random>
#include <memory>
#include <string>
#include <iostream>

#include "util/log_util.h"
#include "util/timing_util.h"
#include "util/thread_pool.h"

// globals
bool g_running = false;
int32_t g_num_exit_signals = 0;
int32_t g_exit_value = 0;

TRACE_GLOBAL_INIT

void exit_handler(int32_t signum ) {
    ++g_num_exit_signals;
    LOG("received interrupt signal={} count={}\n", signum, g_num_exit_signals);
#if USE_TEF
    if (g_num_exit_signals < 3) {
        // toggle tracing
        raise(SIGUSR2);
    } else {
        g_running = false;
    }
#else
    g_running = false;
#endif // USE_TEF

    if (signum == SIGTERM) {
        // SIGTERM indicates clean intentional shutdown
        g_exit_value = 0;
    } else {
        g_exit_value = 1;
    }

    if (g_num_exit_signals > 3) {
        // hint: keep sending signals if process deadlocks
        exit(1);
    }
}

// The purpose of this example is to show how to use the tracing mechanism
// therefore we #ifdef around them as one might do in a real app.
// This is how tracing can be removed at compile time: when USE_TEF is NOT
// defined the tracing macros will exand to NO-OP code which a smart
// compiler will be able to optimize out.
#ifdef USE_TEF

void trace_handler(int32_t signum) {
    // g_trace_consumer was created in TRACE_GLOBAL_INIT
    if (!g_trace_consumer) {
        // we don't yet have a consumer,
        // so we create one and add it to the Tracer
        // which will enable tracing and cause it to start collecting events
        // if it wasn't already
        constexpr uint64_t TRACE_LIFETIME = 10 * timing_util::MSEC_PER_SECOND;
        std::string timestamp = timing_util::get_local_datetime_string(timing_util::get_now_msec());
        // filename = /tmp/YYYYMMDD_HH:MM:SS-trace.json
        std::string filename = fmt::format("/tmp/{}-trace.json", timestamp);
        LOG("START trace file={} lifetime={}msec\n", filename, TRACE_LIFETIME);

        g_trace_consumer = std::make_unique<tef::Trace_to_file>(TRACE_LIFETIME, filename);
        tef::Tracer::instance().add_consumer(g_trace_consumer.get());
        fmt::print("press 'CTRL-C' again to toggle tracing OFF\n");
    } else {
        // we already have consumer,
        // so we interpret this signal as a desire to stop tracing early
        // --> update it with a low expiry and the Tracer will finish it
        // on next mainloop.
        g_trace_consumer->update_expiry(0);
        const std::string& filename = g_trace_consumer->get_filename();
        LOG("STOP trace file={}\n", filename);
        // Note: g_trace_consumer will automatically expire after 10 seconds,
        // even if a second signal never arrives to toggle it off.  This to
        // prevent the trace results file from getting too big: the chrome
        // browser can crash/lock-up when trying to load too much data.
        fmt::print("press 'CTRL-C' one last time to STOP example\n");
    }
}

#endif // USE_TEF

using Data = std::vector<uint32_t>;

// example do_work() method is for consuming CPU cycles
size_t do_work(Data& data) {
    {
        TRACE_CONTEXT("shuffle", "perf");
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(data.begin(), data.end(), g);
    }
    TRACE_CONTEXT("sort", "perf");
    std::sort(data.begin(), data.end());
    return data.size();
}

void run_side_thread() {
    // name this thread
    TRACE_THREAD("side_thread");

    constexpr size_t NUM_DATA = 10000;
    LOG("run_side_thread num_data={}\n", NUM_DATA);
    // initialize
    Data data;
    data.resize(NUM_DATA);
    for (size_t i = 0; i < NUM_DATA; ++i) {
        data[i] = uint32_t(i);
    }
    // loop
    while (g_running) {
        TRACE_CONTEXT("work", "perf");
        size_t data_size = do_work(data);
        // add an 'arg' to the current trace context
        // this is just an example of how to make details
        // visible to the chrome://tracing browser
        TRACE_CONTEXT_ARGS("\"data_size\":{}", data_size);
    }
    LOG("run_side_thread... {}\n", "DONE");
}

void run_another_side_thread() {
    // Note: we don't bother to name this thread, so in the trace browser
    // it will have a numerical name.
    constexpr size_t NUM_DATA = 20000;
    LOG("run_another_side_thread num_data={}\n", NUM_DATA);
    Data data;
    data.resize(NUM_DATA);
    for (size_t i = 0; i < NUM_DATA; ++i) {
        data[i] = i;
    }
    while (g_running) {
        TRACE_CONTEXT("work", "perf");
        size_t data_size = do_work(data);
        TRACE_CONTEXT_ARGS("\"data_size\":{}", data_size);
    }
    LOG("run_another_side_thread... {}\n", "DONE");
}

int32_t main(int32_t argc, char** argv) {
    fmt::print("press 'CTRL-C' to toggle tracing ON\n");
    // name the process
    TRACE_PROCESS("example");

    // name the thread
    TRACE_THREAD("main_thread");

    g_running = true;

    // prepare to catch signals
    signal(SIGINT, exit_handler);
    signal(SIGTERM, exit_handler);
#ifdef USE_TEF
    // register a handler to toggle tracing on/off
    signal(SIGUSR2, trace_handler);
#endif

    constexpr int32_t NUM_THREADS = 2;
    Thread_pool pool(NUM_THREADS);

    // start the worker threads
    pool.enqueue([&] { run_side_thread(); });
    pool.enqueue([&] { run_another_side_thread(); });

    // initialize data for main thread work
    constexpr size_t NUM_DATA = 5000;
    Data data;
    data.resize(NUM_DATA);
    for (size_t i = 0; i < NUM_DATA; ++i) {
        data[i] = i;
    }

    LOG("start mainloop num_data={}\n", NUM_DATA);
    while (g_running) {
        TRACE_CONTEXT("mainloop", "perf");
        {
            // main loop also does work
            TRACE_CONTEXT("work", "perf");
            size_t data_size = do_work(data);
            TRACE_CONTEXT_ARGS("\"data_size\":{}", data_size);
        }

        {
            // We can even trace around the tracer itself
            TRACE_CONTEXT("harvest", "perf");

            // for fun we add an 'arg' to this event:
            // num_events will be visible in chrome://tracing browser
            TRACE_CONTEXT_ARGS("\"num_events\":{}", tef::Tracer::instance().get_num_events());

            // do TRACE harvest/maintenance
            TRACE_MAINLOOP
        }

        {
            TRACE_CONTEXT("sleep", "perf");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // cleanup unfinished tracing (if any) to avoid crash on shutdown
    TRACE_SHUTDOWN;

    // since we have a blocking input thread we explicitly stop the pool
    pool.stop_everything();

    return g_exit_value;
}


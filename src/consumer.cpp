// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "consumer.h"
#include "trace.h"

#include <chrono>
#include <sstream>

// Note: TEFLIB_TRACE_LOG is a hook for printing trace state transitions to stdout.
// To use it, supply your own variable argument macro implementation.  For example:
//#define TEFLIB_TRACE_LOG(fmtstr,...) std::printf(fmtstr, __VA_ARGS__);
//
// Otherwise it defaults to NO-OP
//
#ifndef TEFLIB_TRACE_LOG
#define TEFLIB_TRACE_LOG(fmtstr,...) do{}while(0);
#endif

namespace tef {

// returns msec since first call to get_now_msec()
uint64_t get_now_msec() {
    using namespace std::chrono;
    static uint64_t msec_offset = 0;
    if (msec_offset == 0) {
        msec_offset = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
            - duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() + msec_offset;
}

constexpr uint64_t DEFAULT_CONSUMER_LIFETIME = 10000; // 10 seconds in msec

} // namespace tef

using namespace tef;

Default_consumer::Default_consumer()
    : Consumer(DEFAULT_CONSUMER_LIFETIME), _file("/tmp/trace.json")
{
    _stream.open(_file);
    if (!_stream.is_open()) {
        TEFLIB_TRACE_LOG("failed to open trace file='%s'\n", _file.c_str());
        _file.clear();
    } else {
        TEFLIB_TRACE_LOG("opened trace='%s'\n", _file.c_str());
        _stream << "{\"traceEvents\":[\n";
    }
}

void Default_consumer::start_trace(uint64_t lifetime, const std::string& filename) {
    _file = filename;
    start(lifetime);
}

void Default_consumer::consume_events(const std::vector<std::string>& events) {
    if (_stream.is_open()) {
        for (const auto& event : events) {
            _stream << event << ",\n";
        }
    }
}

void Default_consumer::finish(const std::vector<std::string>& meta_events) {
    Consumer::finish(meta_events);
    if (_stream.is_open()) {
        // TRICK: end with bogus "complete" event sans ending comma
        // (this simplifies consume_event() logic)
        std::string tid_str = Trace::thread_id_as_string();
        uint64_t ts = Trace::instance().now();
        std::string bogus_event = "{\"name\":\"end_of_trace\",\"ph\":\"X\",\"pid\":1,\"tid\":";
        std::ostringstream ss;
        ss << tid_str << ",\"ts\":" << ts << ",\"dur\":" << 1000 << "}";
        bogus_event.append(ss.str());
        _stream << bogus_event << "\n]\n}\n"; // close array instead of comma

        _stream.close();
        TEFLIB_TRACE_LOG("closed trace='%s'\n", _file.c_str());
    }
}

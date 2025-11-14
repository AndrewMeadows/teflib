// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "consumer.h"
#include "trace.h"

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

using namespace tef;

Trace_to_file::Trace_to_file(uint64_t lifetime, const std::string& filename)
    : Consumer(lifetime), _file(filename)
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

void Trace_to_file::consume_events(const std::vector<std::string>& events) {
    if (_stream.is_open()) {
        for (const auto& event : events) {
            _stream << event << ",\n";
        }
    }
}

void Trace_to_file::finish(const std::vector<std::string>& meta_events) {
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

// tef/src/trace.cpp
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "trace.h"

#include <sstream>
#include <fmt/format.h>

// Note: TEFLIB_TRACE_LOG is a hook for printing trace state transitions to stdout.
// To use it, supply your own macro variable argument implementation.  For example:
//#define TEFLIB_TRACE_LOG(fmtstr,...) fmt::print(fmtstr, __VA_ARGS__);
//
// Otherwise it defaults to NO-OP
//
#ifndef TEFLIB_TRACE_LOG
#define TEFLIB_TRACE_LOG(fmtstr,...) do{}while(0);
#endif

using namespace tef::trace;

inline uint64_t get_now_msec() {
    using namespace std::chrono;
    static uint64_t msec_offset = 0;
    if (msec_offset == 0) {
        msec_offset = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
            - duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() + msec_offset;
}

std::unique_ptr<Tracer> Tracer::_instance;

// static
std::string Tracer::thread_id_as_string() {
    // unfortunately, fmt doesn't know how to handle std::thread::id
    // hence this static helper method
    std::stringstream tid_str;
    tid_str << std::this_thread::get_id();
    return tid_str.str();
}

void Tracer::add_event(const std::string& name, const std::string& cat, Phase ph) {
    if (_enabled) {
        std::lock_guard<std::mutex> lock(_event_mutex);
        _events.push_back({name, cat, now(), std::this_thread::get_id(), -1, ph});
    }
}

void Tracer::add_event_with_args(
        const std::string& name,
        const std::string& cat,
        Phase ph,
        const std::string& args) {
    if (_enabled) {
        std::lock_guard<std::mutex> lock(_event_mutex);
        int32_t args_index = (int32_t)(_args.size());
        _args.push_back(args);
        _events.push_back({name, cat, now(), std::this_thread::get_id(), args_index, ph});
    }
}

void Tracer::set_counter(
        const std::string& name,
        const std::string& cat,
        int64_t count)
{
    if (_enabled) {
        std::string args = fmt::format("\"{}\":{}", name, count);
        std::lock_guard<std::mutex> lock(_event_mutex);
        int32_t args_index = (int32_t)(_args.size());
        _args.push_back(args);
        _events.push_back({name, cat, now(), std::this_thread::get_id(), args_index, Phase::Counter});
    }
}

void Tracer::add_meta_event(const std::string& type, const std::string& arg) {
    // Note: 'type' has a finite set of acceptable values
    //   process_name
    //   process_labels
    //   thread_name
    std::string arg_name;
    if (type == "process_name") {
        arg_name = "name";
    } else if (type == "process_labels") {
        arg_name = "labels";
    } else if (type == "thread_name") {
        arg_name = "name";
    }
    if (!arg_name.empty()) {
        // unfortunately, fmt doesn't know how to handle std::thread::id
        // so we format into string
        std::string tid_str = thread_id_as_string();

        // meta_events get formatted to strings immediately
        std::lock_guard<std::mutex> lock(_event_mutex);
        _meta_events.push_back(
            fmt::format(
                "{{\"name\":\"{}\",\"ph\":\"M\",\"pid\":1,\"tid\":{},\"args\":{{\"{}\":\"{}\"}}}}",
                type, tid_str, arg_name, arg));
    }
}

void Tracer::add_meta_event(const std::string& type, uint32_t arg) {
    // Note: 'type' has a finite set of acceptable values
    //   process_sort_index
    //   thread_sort_index
    std::string arg_name;
    if (type == "process_sort_index" || type == "thread_sort_index") {
        std::string tid_str = thread_id_as_string();

        // meta_events get formatted to strings immediately
        std::string event = fmt::format(
                "{{\"name\":\"{}\",\"M\",\"pid\":1,\"tid\":{},\"args\":{{\"sort_index\":{}}}}}",
                type, tid_str, arg);
        std::lock_guard<std::mutex> lock(_event_mutex);
        _meta_events.push_back(event);
    }
}

void Tracer::advance_consumers() {
    if (_events.empty()) {
        return;
    }

    // swap events out
    std::vector<Event> events;
    std::vector<std::string> args;
    {
        std::lock_guard<std::mutex> lock(_event_mutex);
        events.swap(_events);
        args.swap(_args);
    }

    if (_consumers.empty()) {
        return;
    }

    // convert events to strings
    std::vector<std::string> event_strings;
    std::string ph_str("a");
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        // for speed we use fmt formatting where possible...
        ph_str[0] = event.ph;
        std::stringstream stream;
        stream << fmt::format(
                "{{\"name\":\"{}\",\"cat\":\"{}\",\"ph\":\"{}\",\"ts\":{},\"pid\":1",
                event.name, event.cat, ph_str, event.ts);
        // and std::ostream formatting when necessary...
        stream << ",\"tid\":" << event.tid;
        if (event.args_index != -1) {
            stream << fmt::format(",\"args\":{}", args[event.args_index]);
        }
        stream << "}";
        event_strings.push_back(stream.str());
    }

    // consume event strings
    uint64_t now = get_now_msec();
    std::lock_guard<std::mutex> lock(_consumer_mutex);
    std::vector<Tracer::Consumer*> expired_consumers;
    size_t i = 0;
    while (i < _consumers.size()) {
        Tracer::Consumer* consumer = _consumers[i];
        consumer->consume_events(event_strings);
        consumer->check_expiry(now);
        if (consumer->is_expired()) {
            expired_consumers.push_back(consumer);
            size_t last_index = _consumers.size() - 1;
            if (i != last_index) {
                _consumers[i] = _consumers[last_index];
            } else {
                ++i;
            }
            _consumers.pop_back();
            if (_consumers.size() == 0) {
                _enabled = false;
                TEFLIB_TRACE_LOG("trace enabled={}\n", _enabled);
            }
            continue;
        }
        ++i;
    }

    // complete expired consumers with meta_events
    if (expired_consumers.size() > 0) {
        // copy meta_events under lock
        std::vector<std::string> meta_events;
        {
            // Note: we're locking _event_mutex under _consumer_mutex
            // which means we must never lock them in reverse order elsewhere
            // or risk deadlock.
            std::lock_guard<std::mutex> lock(_event_mutex);
            meta_events = _meta_events;
        }
        // feed meta_events to consumers
        for (size_t i = 0; i < expired_consumers.size(); ++i) {
            Tracer::Consumer* consumer = expired_consumers[i];
            consumer->finish(meta_events);
        }
    }
}

// call this for clean shutdown of active consumers
void Tracer::shutdown() {
    {
        std::lock_guard<std::mutex> lock(_consumer_mutex);
        for (size_t i = 0; i < _consumers.size(); ++i) {
            _consumers[i]->update_expiry(0);
        }
    }
    advance_consumers();
}

void Tracer::add_consumer(Tracer::Consumer* consumer) {
    if (consumer) {
        consumer->update_expiry(get_now_msec());
        std::lock_guard<std::mutex> lock(_consumer_mutex);
        _consumers.push_back(consumer);
        if (!_enabled.load()) {
            _enabled = true;
            TEFLIB_TRACE_LOG("trace enabled={}\n", _enabled);
        }
    }
}

void Tracer::remove_consumer(Tracer::Consumer* consumer) {
    // Note: no need to call this unless you're closing the consumer early
    // (e.g. before is_complete)
    std::lock_guard<std::mutex> lock(_consumer_mutex);
    size_t i = 0;
    while (i < _consumers.size()) {
        if (consumer == _consumers[i]) {
            size_t last_index = _consumers.size() - 1;
            if (i != last_index) {
                _consumers[i] = _consumers[last_index];
            } else {
                ++i;
            }
            _consumers.pop_back();
            continue;
        }
        ++i;
    }
    if (_consumers.size() == 0) {
        _enabled = false;
        TEFLIB_TRACE_LOG("trace enabled={}\n", _enabled);
    }
}

Trace_to_file::Trace_to_file(uint64_t lifetime, const std::string& filename)
    : Tracer::Consumer(lifetime), _file(filename)
{
    _stream.open(_file);
    if (!_stream.is_open()) {
        TEFLIB_TRACE_LOG("failed to open trace file='{}'\n", _file);
        _file.clear();
    } else {
        TEFLIB_TRACE_LOG("opened trace='{}'\n", _file);
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
    Tracer::Consumer::finish(meta_events);
    if (_stream.is_open()) {
        // TRICK: end with bogus "complete" event sans ending comma
        // (this simplifies consume_event() logic)
        std::string tid_str = trace::Tracer::thread_id_as_string();
        uint64_t ts = trace::Tracer::instance().now();
        std::string bogus_event = fmt::format(
            "{{\"name\":\"end_of_trace\",\"ph\":\"X\",\"pid\":1,\"tid\":{},\"ts\":{},\"dur\":1000}}",
            tid_str, ts);
        _stream << bogus_event << "\n]\n}\n"; // close array instead of comma

        _stream.close();
        TEFLIB_TRACE_LOG("closed trace='{}'\n", _file);
    }
}

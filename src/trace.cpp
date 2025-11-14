// tef/src/trace.cpp
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "trace.h"
#include "consumer.h"

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

uint64_t tef::get_now_msec() {
    using namespace std::chrono;
    static uint64_t msec_offset = 0;
    if (msec_offset == 0) {
        msec_offset = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
            - duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() + msec_offset;
}

// static
std::string Trace::thread_id_as_string() {
    std::ostringstream tid_str;
    tid_str << std::this_thread::get_id();
    return tid_str.str();
}

void Trace::add_event(uint8_t name, uint8_t categories, Phase ph, uint64_t ts, uint64_t dur) {
    if (_enabled) {
        if (ts == 0)
        {
            ts = Trace::instance().now();
        }
        std::lock_guard<std::mutex> lock(_event_mutex);
        _events.push_back({name, categories, ts, dur, std::this_thread::get_id(), -1, ph});
    }
}

void Trace::add_event_with_args(
        uint8_t name,
        uint8_t categories,
        Phase ph,
        const std::vector<Arg>& args,
        uint64_t ts,
        uint64_t dur)
{
    if (_enabled) {
        if (ts == 0)
        {
            ts = now();
        }
        std::lock_guard<std::mutex> lock(_event_mutex);
        int32_t args_index = int32_t(_arg_lists.size());
        _arg_lists.push_back(args);
        _events.push_back({name, categories, ts, dur, std::this_thread::get_id(), args_index, ph});
    }
}

void Trace::set_counter(
        uint8_t name,
        uint8_t count_name,
        int64_t count)
{
    if (_enabled) {
        std::lock_guard<std::mutex> lock(_event_mutex);
        // the actual count info gets stored in "args"
        std::vector<Arg> args;
        args.push_back({count_name, count});
        int32_t args_index = int32_t(_arg_lists.size());
        _arg_lists.push_back(args);
        // Note: counter events in TEF lack a "cat" field, however to satisfy our own API for
        // the Event struct we recycle "count_name" for the "categories" argument.  When it
        // is time to generate the report the "categories" value won't be used for Counters.
        _events.push_back({name, count_name, now(), 0, std::this_thread::get_id(), args_index, Phase::Counter});
    }
}

void Trace::add_meta_event(const std::string& type, const std::string& arg) {
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
        std::string tid_str = thread_id_as_string();

        // meta_events get formatted to strings immediately
        std::lock_guard<std::mutex> lock(_event_mutex);
        std::string s = "{\"name\":\"";
        s.append(type);
        s.append("\",\"ph\":\"M\",\"pid\":1,\"tid\":");
        s.append(tid_str);
        s.append(",\"args\":{\"");
        s.append(arg_name);
        s.append("\":\"");
        s.append(arg);
        s.append("\"}}");
        _meta_events.push_back(s);
    }
}

void Trace::add_meta_event(const std::string& type, uint32_t arg) {
    // Note: 'type' has a finite set of acceptable values
    //   process_sort_index
    //   thread_sort_index
    std::string arg_name;
    if (type == "process_sort_index" || type == "thread_sort_index") {
        std::string tid_str = thread_id_as_string();

        // format meta_events to strings immediately
        std::string event = "{\"name\":\"";
        event.append(type);
        event.append("\",\"ph\":\"M\",\"pid\":1,\"tid\":");
        event.append(tid_str);
        event.append(",\"args\":{\"sort_index\":");
        std::ostringstream ss;
        ss << arg;
        event.append(ss.str());
        event.append("}}");
        std::lock_guard<std::mutex> lock(_event_mutex);
        _meta_events.push_back(event);
    }
}

void Trace::advance_consumers() {
    if (_events.empty()) {
        return;
    }

    // swap events out
    std::vector<Event> events;
    std::vector< std::vector<Arg> > arg_lists;
    {
        std::lock_guard<std::mutex> lock(_event_mutex);
        events.swap(_events);
        arg_lists.swap(_arg_lists);
    }

    if (_consumers.empty()) {
        return;
    }

    // According to this document:
    //     https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview?tab=t.0
    // an example event JSON string looks like this:
    // {
    //    "name": "myName",
    //    "cat": "category,list",
    //    "ph": "B",
    //    "ts": 12345,
    //    "pid": 123,
    //    "tid": 456,
    //    "args": {
    //      "someArg": 1,
    //      "anotherArg": {
    //        "value": "my value"
    //      }
    //    }
    //  }
    //
    // Where:
    //   name = human readable name for the event
    //   cat = comma separated words used for filtering
    //   ph = phase type
    //   ts = timestamp
    //   dur = duration
    //   tid = thread_id
    //   pid = process_id
    //   args = JSON map of special info

    // convert events to strings
    std::vector<std::string> event_strings;
    std::string ph_str("a");
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        const std::string& name = _registered_strings[event.name];
        const std::string& categories = _registered_strings[event.categories];
        ph_str[0] = event.ph;
        std::ostringstream stream;
        if (event.ph == Phase::Complete) {
            stream << "{\"name\":\"" << name << "\""
                << ",\"cat\":\"" << categories << "\""
                << ",\"ph\":\"" << ph_str << "\""
                << ",\"ts\":" << event.ts
                << ",\"dur\":" << event.dur
                << ",\"pid\":1";
        } else if (event.ph == Phase::Counter) {
            // counters don't have "cat" or "dur"
            stream << "{\"name\":\"" << name << "\""
                << ",\"ph\":\"" << ph_str << "\""
                << ",\"ts\":" << event.ts
                << ",\"pid\":1";
        } else {
            stream << "{\"name\":\"" << name << "\""
                << ",\"cat\":\"" << categories << "\""
                << ",\"ph\":\"" << ph_str << "\""
                << ",\"ts\":" << event.ts
                << ",\"pid\":1";
        }
        stream << ",\"tid\":" << event.tid;
        if (event.args_index != -1) {
            std::vector<Arg>& args = arg_lists[event.args_index];
            // build the "args" string which should be valid JSON:
            //    "args": {
            //      "someArg": 1,
            //      "anotherArg": {
            //        "value": "my value"
            //      }
            //    }
            //
            std::ostringstream s;
            stream << ",\"args\":{";
            for (size_t i = 0; i < args.size() - 1; ++i)
            {
                stream << args[i].json_str(_registered_strings);
                stream << ",";
            }
            if (args.size() > 0)
            {
                stream << args[args.size() - 1].json_str(_registered_strings);
            }
            stream << "}";
        }
        stream << "}";
        event_strings.push_back(stream.str());
    }

    // consume event strings
    uint64_t now = get_now_msec();
    std::lock_guard<std::mutex> lock(_consumer_mutex);
    std::vector<Consumer*> expired_consumers;
    size_t i = 0;
    while (i < _consumers.size()) {
        Consumer* consumer = _consumers[i];
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
            Consumer* consumer = expired_consumers[i];
            consumer->finish(meta_events);
        }
    }
}

// call this for clean shutdown of active consumers
void Trace::shutdown() {
    {
        std::lock_guard<std::mutex> lock(_consumer_mutex);
        for (size_t i = 0; i < _consumers.size(); ++i) {
            _consumers[i]->update_expiry(0);
        }
    }
    advance_consumers();
}

void Trace::add_consumer(Consumer* consumer) {
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

void Trace::register_string(uint8_t index, const std::string& str) {
    // Note: _registered_strings is NOT behind a mutex, which means it isn't thread-safe
    // to register strings in a multi-threaded fashion.  Do all string registration early
    // on the main thread.
    _registered_strings[index] = str;
}

void Trace::remove_consumer(Consumer* consumer) {
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

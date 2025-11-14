// trace.h
//
// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include <sstream>

#include "singleton.h"

namespace tef {

// Forward declarations
class Consumer;
class Trace_to_file;
class Trace;

// The goal here is to provide a fast+simple trace tool rather than a
// complete one.  As a consequence not all Phase types are supported.
//
enum Phase : char {
    // supported:
    DurationBegin = 'B',
    DurationEnd = 'E',
    Counter = 'C',
    Metadata = 'M',
    Complete = 'X',

    // unsupported:
    Instant = 'i',

    AsyncNestableStart = 'b',
    AsyncNestableInstant = 'n',
    AsyncNestableEnd = 'e',

    FlowStart = 's',
    FlowStep = 't',
    FlowEnd = 'f',

    Sample = 'P',

    ObjectCreated = 'N',
    ObjectSnapshot = 'O',
    ObjectDestroyed = 'D',

    MemoryDumpGlobal = 'V',
    MemoryDumpProcess = 'v',

    Mark = 'R',

    ClockSync = 'c',

    ContextEnter = '(',
    ContextLeave = ')'
};

class Trace : public Singleton<Trace> {
public:
    using Variant = std::variant<std::monostate, int32_t, uint32_t, int64_t, uint64_t, float, double, std::string_view>;
    using KeyedVariant = std::pair< uint8_t, Variant >;

    class Arg {
    public:
        Arg(uint8_t key, Variant value) : _value(value), _key(key) {}

        std::string json_str(const std::vector<std::string>& registered_strings) {
            if (_json_string.empty())
            {
                // _json_string hasn't been created yet, so we do it now
                const std::string& key_str(registered_strings[_key]);
                _json_string = "\"" + key_str + "\":";

                std::visit([this](auto arg) {
                    using T = decltype(arg);
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        _json_string += "null";
                    } else if constexpr (std::is_same_v<T, std::string_view>) {
                        _json_string += "\"" + std::string(arg) + "\"";
                    } else {
                        _json_string += std::to_string(arg);
                    }
                }, _value);
            }
            return _json_string;
        }

    public:
        std::string _json_string;
        Variant _value;
        uint8_t _key;
    };

    // Context measures timestamp in ctor
    // and adds a Phase::Complete event to Trace in dtor
    class Context {
    public:
        friend Trace;

        Context(uint8_t name, uint8_t categories)
            : _name(name), _categories(categories)
        {
            _ts= Trace::instance().now();
        }

        ~Context() {
            if (_args.empty()) {
                Trace::instance().add_event(_name, _categories, Phase::Complete, _ts, Trace::instance().now() - _ts);
            } else {
                Trace::instance().add_event_with_args(_name, _categories, Phase::Complete, _args, _ts, Trace::instance().now() - _ts);
            }

        }

        // add an optional Arg to this Context
        void add_arg(const KeyedVariant& kv) { _args.push_back({kv.first, kv.second}); }

    protected:
        std::vector<Arg> _args; // optional list of name_index:variant_value pairs
        uint64_t _ts; // timestamp
        uint8_t _name; // index to registered name string
        uint8_t _categories; // index to registered string of comma separated category words
    };

    // helper
    static std::string thread_id_as_string();

    Trace() : _start_time(std::chrono::high_resolution_clock::now()) {
        _registered_strings.resize(256);
    }

    uint64_t now() const {
        uint64_t t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - _start_time).count();
        // BUG: chrome://tracing sometimes won't correctly organize embedded
        // events with simultaneous start times!
        // WORKAROUND: We make sure now() always returns an increasing value.
        // This introduces slight error on measurements and an effective event
        // rate limit of about 0.5MHz.  If you need sub-microsecond accuracy
        // or if your events are arriving faster than 0.5MHz then teflib is
        // probably NOT the right tool for your job.
        if (t <= _last_t)
        {
            t = _last_t + 1;
        }
        _last_t = t;
        return t;
    }

    // simple method for manually adding an event without args
    void add_event(uint8_t name, uint8_t categories, Phase ph, uint64_t ts=0, uint64_t dur=0);

    // simple method for manually adding events with already formatted arg string
    // (needs to be properly formatted JSON map).
    void add_event_with_args(
            uint8_t name,
            uint8_t categories,
            Phase ph,
            const std::vector<Arg>& args,
            uint64_t ts=0, uint64_t dur=0);

    // adds a "Counter" event
    void set_counter(uint8_t name, uint8_t count_name, int64_t count);

    // type = process_name, process_labels, or thread_name
    void add_meta_event(const std::string& type, const std::string& arg);

    // type = process_sort_index, or thread_sort_index
    void add_meta_event(const std::string& type, uint32_t arg);

    void add_consumer(Consumer* consumer);
    void advance_consumers();
    void shutdown();

    // don't call remove_consumer() unless you know what you're doing
    // (e.g. shutting down before consumers are complete)
    void remove_consumer(Consumer* consumer);

    size_t get_num_events() const { return _events.size(); }

    // Register a string for use with index arguments in Context or add_event.
    // The registered string will remain valid for the lifetime of the Tracer.
    // This is useful for dynamic strings that need to outlive the Context lifetime.
    void register_string(uint8_t index, const std::string& str);

    mutable std::mutex _event_mutex;
    mutable std::mutex _consumer_mutex;
    std::chrono::high_resolution_clock::time_point _start_time;

    // Note: Event does not store 'pid' because we assume
    // all events are for the same process.
    // Event uses uint8_t indices for name/categories to avoid allocations.
    // The indices reference strings in _registered_strings.
    struct Event {
        uint8_t name;
        uint8_t categories;
        uint64_t ts;
        uint64_t dur;
        std::thread::id tid;
        int32_t args_index;
        Phase ph;
    };

private:
    std::vector<Consumer*> _consumers;

    std::vector<Event> _events;
    std::vector<std::string> _meta_events;
    std::vector< std::vector<Arg> > _arg_lists;
    std::vector<std::string> _registered_strings; // pre-allocated to 256 elements
    std::atomic<bool> _enabled { false };
    mutable uint64_t _last_t { 0 };

    Trace(Trace const&); // Don't Implement
    void operator=(Trace const&); // Don't implement
};

} // namespace tef

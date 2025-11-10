// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
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
// (4) In your main loop, call TRACE_MAINLOOP repeatedly on each iteration
//
// (5) After mainloop but before exit add the macro: TRACE_SHUTDOWN
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
//
//     #include "trace.h"
//
//     TRACE_GLOBAL_INIT
//
//     // using constexpr for trace string indices isn't required
//     // but simplifies multi-threaded context of the indices
//     // and minimizes the compiled runtime overhead
//     constexpr MY_FUNCTION = 0;
//     constexpr WORK = 1;
//
//     void register_trace_strings {
//         TRACE_REGISTER_STRING(MY_FUNCTION, "my_function");
//         TRACE_REGISTER_STRING(WORK, "work");
//     }
//
//     void my_function() {
//         TRACE_CONTEXT(MY_FUNCTION, WORK);
//         // ... function body ...
//     }
//
//     int main() {
//         // init trace strings
//         register_trace_strings();
//
//         // Start tracing to file for 5 seconds
//         TRACE_START(5000, "trace.json");
//
//         while (running) {
//             TRACE_MAINLOOP
//             my_function();
//             // ... other work ...
//         }
//
//         TRACE_SHUTDOWN
//         return 0;
//     }

#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

// to avoid dependency on fmt compile with -DNO_FMT
#define NO_FMT
#ifdef NO_FMT
#include <sstream>
#else
#include <fmt/format.h>
#endif

namespace tef {

constexpr uint64_t DISTANT_FUTURE = uint64_t(-1);
constexpr uint64_t MSEC_PER_SECOND = 1e3;

// Maximum duration for a single trace session (to prevent chrome://tracing from crashing)
constexpr uint64_t MAX_TRACE_CONSUMER_LIFETIME = 10 * MSEC_PER_SECOND;

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

uint64_t get_now_msec();


// Note: Tracer is a singleton
class Tracer {
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
#ifdef NO_FMT
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
#else
                std::visit([this, &key_str](auto arg) {
                    using T = decltype(arg);
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        _json_string = fmt::format("\"{}\": null", key_str);
                    } else if constexpr (std::is_same_v<T, std::string_view>) {
                        _json_string = fmt::format("\"{}\": \"{}\"", key_str, arg);
                    } else {
                        _json_string = fmt::format("\"{}\": {}", key_str, arg);
                    }
                }, _value);
#endif
            }
            return _json_string;
        }

    public:
        std::string _json_string;
        Variant _value;
        uint8_t _key;
    };

    // Context measures timestamp in ctor
    // and adds a Phase::Complete event to Tracer in dtor
    class Context {
    public:
        friend Tracer;

        Context(uint8_t name, uint8_t categories)
            : _name(name), _categories(categories)
        {
            _ts= Tracer::instance().now();
        }

        ~Context() {
            if (_args.empty()) {
                Tracer::instance().add_event(_name, _categories, Phase::Complete, _ts, Tracer::instance().now() - _ts);
            } else {
                Tracer::instance().add_event_with_args(_name, _categories, Phase::Complete, _args, _ts, Tracer::instance().now() - _ts);
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

    // To harvest trace events the pattern is:
    // (1) Create a Consumer and give pointer to Tracer
    // (2) Override consume_events() to do what you want with events
    // (3) When consumer is COMPLETE close and delete
    // (Tracer automatically removes consumer before COMPLETE)
    class Consumer {
    public:
        friend class Tracer;

        enum State : uint8_t {
            ACTIVE,  // collecting events
            EXPIRED, // lifetime is up
            COMPLETE // all done (has collected meta_events)
        };

        Consumer(uint64_t lifetime) {
            // Note: lifetime is limited because the chrome://tracing tool
            // can crash when browsing very large files
            if (lifetime > MAX_TRACE_CONSUMER_LIFETIME) {
                lifetime = MAX_TRACE_CONSUMER_LIFETIME;
            }
            _lifetime = lifetime;
        }

        virtual ~Consumer() {}

        // override this pure virtual method to Do Stuff with events
        // each event will be a JSON string as per the google tracing API
        virtual void consume_events(const std::vector<std::string>& events) = 0;

        // called by Tracer on add
        // but can also be used to change expiry on the fly
        void update_expiry(uint64_t now) { _expiry = now + _lifetime; }

        bool is_expired() const { return _state == State::EXPIRED; };
        bool is_complete() const { return _state == State::COMPLETE; }

        // called by Tracer after consume_events
        void check_expiry(uint64_t now) {
            if (now > _expiry) {
                _state = EXPIRED;
            }
        }

        // called by Tracer after expired
        virtual void finish(const std::vector<std::string>& meta_events) {
            assert(_state == State::EXPIRED);
            consume_events(meta_events);
            _state = State::COMPLETE;
        }

    protected:
        uint64_t _lifetime; // msec
        uint64_t _expiry { DISTANT_FUTURE };
        State _state { State::ACTIVE };
    };

    static Tracer& instance() {
        static std::unique_ptr<Tracer> _instance(new Tracer());
        return (*_instance);
    }

    // helper
    static std::string thread_id_as_string();

    Tracer() : _start_time(std::chrono::high_resolution_clock::now()) {
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

private:
    static std::unique_ptr<Tracer> _instance;
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

    std::vector<Consumer*> _consumers;

    std::vector<Event> _events;
    std::vector<std::string> _meta_events;
    std::vector< std::vector<Arg> > _arg_lists;
    std::vector<std::string> _registered_strings; // pre-allocated to 256 elements
    std::atomic<bool> _enabled { false };
    mutable uint64_t _last_t { 0 };

    Tracer(Tracer const&); // Don't Implement
    void operator=(Tracer const&); // Don't implement
};


// Trace_to_file is a simple consumer for saving events to file
class Trace_to_file : public Tracer::Consumer {
public:
    Trace_to_file(uint64_t lifetime, const std::string& filename);
    void consume_events(const std::vector<std::string>& events) final override;
    void finish(const std::vector<std::string>& meta_events) final override;
    bool is_open() const { return _stream.is_open(); }
    const std::string& get_filename() const { return _file; }
private:
    std::string _file;
    std::ofstream _stream;
};

} // namespace tef

    // We use 'do{}while(0)' as the no-op because:
    // (1) Most compilers will optimize it out
    // (2) It is a single expression and should not unexpectedly break contexts
    //     (e.g. when used in a single-line-'if' context without braces)
    #define TRACE_NOOP do{}while(0)

#ifdef USE_TEF

    // Always invoke TRACE_GLOBAL_INIT in main global space.
    // It initializes g_trace_consumer_ptr which points at the glboal Trace_to_file instance
    // when tracing is active, or to nullptr when not.
    #define TRACE_GLOBAL_INIT std::unique_ptr<tef::Trace_to_file> g_trace_consumer_ptr;

    // Register a string for use with TRACE_CONTEXT.
    // The index must be a uint8_t value (0-255).
    #define TRACE_REGISTER_STRING(index, str) ::tef::Tracer::instance().register_string(index, str)

    // Start tracing to filename for a duration of lifetime_msec.
    // After lifetime_msec the Trace_to_file instance will be "complete" and will be
    // deleted inside TRACE_MAINLOOP.
    #define TRACE_START(lifetime_msec, filename) \
        g_trace_consumer_ptr = std::make_unique<tef::Trace_to_file>(lifetime_msec, filename); \
        ::tef::Tracer::instance().add_consumer(g_trace_consumer_ptr.get());

    // Check if tracing is currently active
    #define TRACE_IS_ACTIVE() (g_trace_consumer_ptr != nullptr)

    // Stop tracing early by setting expiry to 0
    #define TRACE_STOP_EARLY() if (g_trace_consumer_ptr) g_trace_consumer_ptr->update_expiry(0);

    // Get the current trace filename (returns empty string if not tracing)
    #define TRACE_GET_FILENAME() (g_trace_consumer_ptr ? g_trace_consumer_ptr->get_filename() : std::string())

    // Process accumulated trace Events and check expiry of the active trace
    #define TRACE_MAINLOOP ::tef::Tracer::instance().advance_consumers(); if(g_trace_consumer_ptr && g_trace_consumer_ptr->is_complete()) g_trace_consumer_ptr.reset();

    // Stop tracing and make sure tracing doesn't leak memory.
    #define TRACE_SHUTDOWN ::tef::Tracer::instance().shutdown(); if (g_trace_consumer_ptr) g_trace_consumer_ptr.reset();

    // Invoke TRACE_PROCESS once during init to supply a name for the process.
    // Otherwise the process will be given some arbitrary numerical name.
    #define TRACE_PROCESS(name) ::tef::Tracer::instance().add_meta_event("process_name", name);

    // Invoke TRACE_THREAD once per thread to name the thread in the final report.
    // Otherwise the thread will be given a numerical name.
    #define TRACE_THREAD(name) ::tef::Tracer::instance().add_meta_event("thread_name", name);

    // Invoke TRACE_THREAD_SORT once per thread to assign a sorting index when the Trace data
    // is displayed in the trace browser. Otherwise the threads will be sorted in arbitrary order.
    #define TRACE_THREAD_SORT(index) ::tef::Tracer::instance().add_meta_event("thread_sort_index", index);

    // Create trace Events inside the local context.
    // name = index to the registered context name string
    // categories = index to the registered categories string
    #define TRACE_CONTEXT(name, categories) ::tef::Tracer::Context _tef_context_(name, categories);

    // Add an arg that will be added to the Event in the stream when the report is generated.
    #define TRACE_CONTEXT_ARG(name, value)  _tef_context_.add_arg({name, value});

    // Add a Counter
    // TODO: figure out how to support multiple counts
    #define TRACE_COUNTER(name, count_name, count) ::tef::Tracer::instance().set_counter(name, count_name, int64_t(count));

    // use TRACE_BEGIN/END when you know what you're doing
    // and when TRACE_CONTEXT does not quite do what you need
    #define TRACE_BEGIN(name, categories) ::tef::Tracer::instance().add_event(name, categories, ::tef::Phase::DurationBegin);
    #define TRACE_END(name, categories) ::tef::Tracer::instance().add_event(name, categories, ::tef::Phase::DurationEnd);

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

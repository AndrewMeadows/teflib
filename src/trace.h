// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <fstream>
#include <cassert>

namespace tef::trace {
constexpr uint64_t DISTANT_FUTURE = uint64_t(-1);
constexpr uint64_t MSEC_PER_SECOND = 1e3;

// The goal here is to provide a fast+simple trace tool rather than a
// complete one.  As a consequence not all Phase types are supported.
// Unsupported phases are those that require fields outside of this
// minimal subset:
//
//   name = human readable name for the event
//   cat = comma separated strings used for filtering
//   ph = phase type
//   ts = timestamp
//   tid = thread_id
//   pid = process_id
//   args = JSON string of special info
//
// For example, the 'Complete' event requires a 'dur' field and the
// 'Async*' events use 'id'.
//
enum Phase : char {
    // supported:
    DurationBegin = 'B',
    DurationEnd = 'E',
    Counter = 'C',
    Metadata = 'M',

    // unsupported:
    Complete = 'X',
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

// Note: Tracer is a singleton
class Tracer {
public:

    // To harvest trace events the pattern is:
    // (1) create a Consumer and give pointer to Tracer
    // (2) override consume_events() to do what you want with events
    // (3) when consumer is COMPLETE close and delete
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
            constexpr uint64_t MAX_TRACE_CONSUMER_LIFETIME = 10 * MSEC_PER_SECOND;
            if (lifetime > MAX_TRACE_CONSUMER_LIFETIME) {
                lifetime = MAX_TRACE_CONSUMER_LIFETIME;
            }
            _lifetime = lifetime;
        }

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

    Tracer() : _start_time(std::chrono::high_resolution_clock::now()) { }

    uint64_t now() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - _start_time).count();
    }

    void add_event(const std::string& name, const std::string& cat, Phase ph);
    void add_event_with_args(
            const std::string& name,
            const std::string& cat,
            Phase ph,
            const std::string& args);
    void set_counter(const std::string& name, const std::string& cat, int64_t count);

    // type = process_name, process_lables, or thread_name
    void add_meta_event(const std::string& type, const std::string& arg);

    // type = process_sort_index, or thread_sort_index
    void add_meta_event(const std::string& type, uint32_t arg);

    void add_consumer(Consumer* consumer);
    void advance_consumers();
    void shutdown();

    // don't call remove_consumer() unless you know what you're doing
    // (e.g. shutting down before consumers are complete)
    void remove_consumer(Consumer* consumer);

private:
    static std::unique_ptr<Tracer> _instance;
    mutable std::mutex _event_mutex;
    mutable std::mutex _consumer_mutex;
    std::chrono::high_resolution_clock::time_point _start_time;

    // Note: Event does not store 'pid' because we assume
    // all events are for the same process.
    struct Event {
        std::string name;
        std::string cat;
        uint64_t ts;
        std::thread::id tid;
        int32_t args_index;
        Phase ph;
    };

    std::vector<Consumer*> _consumers;

    std::vector<Event> _events;
    std::vector<std::string> _meta_events;
    std::vector<std::string> _args;
    std::atomic<bool> _enabled { false };

    Tracer(Tracer const&); // Don't Implement
    void operator=(Tracer const&); // Don't implement
};

// Trace_context creates a DurationBegin event in ctor
// and an DurationEnd event in dtor
class Trace_context {
public:
    Trace_context(const std::string& name, const std::string& cat)
        : _name(name), _cat(cat)
    {
        Tracer::instance().add_event(_name, _cat, Phase::DurationBegin);
    }
    ~Trace_context() {
        Tracer::instance().add_event(_name, _cat, Phase::DurationEnd);
    }
    std::string _name;
    std::string _cat;
};

// Trace_to_file is a simple consumer for saving events to file
class Trace_to_file : public Tracer::Consumer {
public:
    Trace_to_file(uint64_t lifetime, const std::string& filename);
    void consume_events(const std::vector<std::string>& events) final override;
    void finish(const std::vector<std::string>& meta_events) final override;
    bool is_open() const { return _stream.is_open(); }
private:
    std::string _file;
    std::ofstream _stream;
};

} // namespace tef

#ifdef USE_TEF

    // Usage pattern:
    //
    // (1) Add TRACE_CONTEXT where you want to measure timing.
    //     Only one per scope, but you can add to embedded scopes.
    //
    // (2) Implement a custom Consumer class to handle trace events
    //     or use trace::Trace_to_file consumer to write to file.
    //
    // (3) To start tracing: add consumer to Tracer. This will enable trace events.
    //
    // (4) Every so often (e.g. inside mainloop):
    //
    //   (4A) call TRACE_ADVANCE_CONSUMERS
    //
    //   (4B) when consumer->is_complete() --> delete it
    //        (it has automatically been removed from Tracer)
    //
    // (5) When Tracer has no consumers it stops collecting trace
    //     events.
    //
    // (6) Add TRACE_SHUTDOWN after mainloop exits in case there
    //     are unfinished consumers on shutdown.
    //
    // (7) Compile with -DUSE_TEF


    // use this inside your mainloop
    #define TRACE_ADVANCE_CONSUMERS ::tef::trace::Tracer::instance().advance_consumers()

    // use this after mainloop, before exit
    #define TRACE_SHUTDOWN ::tef::trace::Tracer::instance().shutdown()

    // use these to add meta_events
    #define TRACE_PROCESS(name) ::tef::trace::Tracer::instance().add_meta_event("process_name", name)
    #define TRACE_THREAD(name) ::tef::trace::Tracer::instance().add_meta_event("thread_name", name)
    #define TRACE_THREAD_SORT(index) ::tef::trace::Tracer::instance().add_meta_event("thread_sort_index", index)

    // use TRACE_CONTEXT for easy Duration events
    #define TRACE_CONTEXT(name, cat) ::tef::trace::Trace_context trace_context(name, cat)

    // use TRACE_BEGIN/END only if you know what you're doing
    // and TRACE_CONTEXT doesn't work for you
    #define TRACE_BEGIN(name, cat) ::tef::trace::Tracer::instance().add_event(name, cat, ::tef::trace::Phase::DurationBegin)
    #define TRACE_END(name, cat) ::tef::trace::Tracer::instance().add_event(name, cat, ::tef::trace::Phase::DurationBegin)

#else
    // all macros are no-ops
    //
    // we use 'do{}while(0)' as the no-op because:
    //
    // (1) most compilers will optimize it out
    // (2) it is a single expression and should not unexpectedly break contexts
    //     (e.g. when used in a single-line 'if' context without brackets)

    #define TRACE_ADVANCE_CONSUMERS do{}while(0)
    #define TRACE_SHUTDOWN do{}while(0)

    #define TRACE_PROCESS(name) do{}while(0)
    #define TRACE_THREAD(name) do{}while(0)
    #define TRACE_THREAD_SORT(index) do{}while(0)

    #define TRACE_CONTEXT(name, cat) do{}while(0)
    #define TRACE_BEGIN(name, cat) do{}while(0)
    #define TRACE_END(name, cat) do{}while(0)

#endif // USE_TEF

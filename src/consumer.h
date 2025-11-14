// consumer.h
//
//  teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include "singleton.h"

namespace tef {

// Forward declaration

constexpr uint64_t DISTANT_FUTURE = uint64_t(-1);
constexpr uint64_t MSEC_PER_SECOND = 1e3;

// Maximum duration for a single trace session (to prevent chrome://tracing from crashing)
constexpr uint64_t MAX_TRACE_CONSUMER_LIFETIME = 10 * MSEC_PER_SECOND;

// helper
// returns msec since first call to get_now_msec()
uint64_t get_now_msec();

// To harvest trace events the pattern is:
// (1) Create a Consumer and give pointer to the Trace instance
// (2) Override consume_events() to do what you want with events
// (3) When consumer is COMPLETE close and delete
// (Trace automatically removes consumer before COMPLETE)
class Consumer {
public:
    enum State : uint8_t {
        ACTIVE,  // collecting events
        EXPIRED, // lifetime is up
        COMPLETE // all done (has collected meta_events)
    };

    Consumer(uint64_t lifetime) {
        // Note: lifetime is limited because the chrome://tracing tool
        // can crash when browsing very large files
        set_lifetime(lifetime);
    }

    void set_lifetime(uint64_t lifetime) {
        if (lifetime > MAX_TRACE_CONSUMER_LIFETIME) {
            lifetime = MAX_TRACE_CONSUMER_LIFETIME;
        }
        _lifetime = lifetime;
    }

    virtual ~Consumer() {}

    virtual void start(uint64_t lifetime = 0) {
        if (_state != State::ACTIVE) {
            if (lifetime > 0)
                set_lifetime(lifetime);
            _expiry = get_now_msec() + _lifetime;
            _state = State::ACTIVE;
        }
    }

    virtual void stop() {
        expire();
    }

    // override this pure virtual method to Do Stuff with events
    // each event will be a JSON string as per the google tracing API
    virtual void consume_events(const std::vector<std::string>& events) = 0;

    void expire() {
        if (_state == State::ACTIVE) {
            _expiry = 0;
            _state == State::EXPIRED;
        }
    }

    bool is_active() const { return _state == State::ACTIVE; }
    bool is_expired() const { return _state == State::EXPIRED; };
    bool is_complete() const { return _state == State::COMPLETE; }

    // called by Trace after consume_events
    void check_expiry(uint64_t now) {
        if (now > _expiry) {
            _state = EXPIRED;
        }
    }

    // called by Trace after expired
    virtual void finish(const std::vector<std::string>& meta_events) {
        assert(_state == State::EXPIRED);
        consume_events(meta_events);
        _state = State::COMPLETE;
    }

protected:
    uint64_t _lifetime; // msec
    uint64_t _expiry { DISTANT_FUTURE };
    State _state { State::COMPLETE };
};

// Default_consumer is a simple consumer for saving events to file
class Default_consumer : public Consumer, public Singleton<Default_consumer> {
public:
    Default_consumer();
    void start_trace(uint64_t lifetime, const std::string& filename);
    void consume_events(const std::vector<std::string>& events) final override;
    void finish(const std::vector<std::string>& meta_events) final override;
    bool is_open() const { return _stream.is_open(); }
    const std::string& get_filename() const { return _file; }
private:
    std::string _file;
    std::ofstream _stream;
};

} // namespace tef

// teflib - Trace Event Format library
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

namespace tef {

// Forward declaration
//class Trace;

constexpr uint64_t DISTANT_FUTURE = uint64_t(-1);
constexpr uint64_t MSEC_PER_SECOND = 1e3;

// Maximum duration for a single trace session (to prevent chrome://tracing from crashing)
constexpr uint64_t MAX_TRACE_CONSUMER_LIFETIME = 10 * MSEC_PER_SECOND;

// To harvest trace events the pattern is:
// (1) Create a Consumer and give pointer to the Trace instance
// (2) Override consume_events() to do what you want with events
// (3) When consumer is COMPLETE close and delete
// (Trace automatically removes consumer before COMPLETE)
class Consumer {
public:
    //friend class Trace;

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

    // called by Trace on add
    // but can also be used to change expiry on the fly
    void update_expiry(uint64_t now) { _expiry = now + _lifetime; }

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
    State _state { State::ACTIVE };
};

// Trace_to_file is a simple consumer for saving events to file
class Trace_to_file : public Consumer {
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

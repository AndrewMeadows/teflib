// teflib - Trace Event Format library
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <memory>
#include <mutex>

namespace tef {

// Thread-safe Singleton base class using CRTP (Curiously Recurring Template Pattern)
template<typename T>
class Singleton {
public:
    // Get the singleton instance (thread-safe)
    static T& instance() {
        std::call_once(_init_flag, []() {
            _instance.reset(new T());
        });
        return *_instance;
    }

    // Delete copy constructor and assignment operator
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton() = default;
    virtual ~Singleton() = default;

private:
    static std::unique_ptr<T> _instance;
    static std::once_flag _init_flag;
};

// Initialize static members
template<typename T>
std::unique_ptr<T> Singleton<T>::_instance = nullptr;

template<typename T>
std::once_flag Singleton<T>::_init_flag;

} // namespace tef

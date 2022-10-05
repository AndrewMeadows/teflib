# teflib
Lightweight C++ tracing library to produce data in Google's
[Trace Event Format](https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit#heading=h.yr4qxyxotyw)
(**TEF**).

## What is it?
The Chrome browser has a tool at [chrome://tracing](chrome://tracing)
which allows for easy visualization and examination of timing measurements.
It is most commonly used for webpage JavaScript performance analysis however it will load any correctly formatted data file.

**teflib** is a small utility for measuring events in a C++ project and then properly formatting that data so it can be loaded by the [chrome://tracing](chrome://tracing) tool.
It does not yet support 100% of the TEF feature set, but it is complete enough for many purposes.

## How to use teflib?
**teflib** consists of two files: `trace.h` and `trace.cpp`.  It requires at least **C++-14** and depends on the [fmt](https://fmt.dev/latest/index.html) library.
The files can be embedded in your own project, or they can be compiled into a library and linked as a dependency.

The simplest pattern is to sprinkle teflib's **helper macros** in your code:
```
// main.cpp

...

// in main global context:
# include "path/to/teflib/trace.h"
TRACE_GLOBAL_INIT;

...

int main() {
    // optional: at the start of main():
    TRACE_PROCESS("application name");

    // optional: at start of any thread:
    TRACE_THREAD("thread name");
 
    ...
   
    while (looping) { 
        // in main loop:
        TRACE_MAINLOOP;

        ...
    } // end mainloop

    // after main loop, before exit:
    TRACE_CLEANUP;
    
    ...

    return value;
} // end of main()
```

Wherever you want to measure duration add a `TRACE_CONTEXT()` macro, one per context:
```
  ...
    while (something_is_true) {
        TRACE_CONTEXT("loop", "category");

        ...

        // use curly braces to define context scope for more detailed TRACE_CONTEXT
        {
            TRACE_CONTEXT("thing one", "category");
            do_thing_one();
        }
        {
            TRACE_CONTEXT("thing_two", "category");
            do_thing_two();
        }
        ...

        // optionally add 'args' to the context to expose extra to the tracing browser
        TRACE_CONTEXT_ARGs("\"num_things\":", num_things);
    }
```

There is a little more to it because tracing shouldn't be enabled by default: you would normally toggle it on/off with one or more triggers.
There are many ways to do this and the best way will depend on your application's interface.
Please examine the teflib `example` source code to see one way to do it.

## To build:
1. In `teflib/` main directory:
  1. `mkdir build`
  1. `cd build`
1. In `teflib/build/` directory:
  1. `cmake -DCMAKE_BUILD_TYPE=Release ../`
  1. `make`


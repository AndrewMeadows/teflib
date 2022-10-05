# teflib example
A simple multi-threaded CLI application instrumented with teflib for trace measurements.

## Build the example:
1. In `teflib/` main directory:
  1. `mkdir build`
  1. `cd build`
1. In `teflib/build/` directory:
  1. `cmake -DCMAKE_BUILD_TYPE=Release
  1. `make`

## Run the example:
1. In `teflib/build/example/` run the executable: `./example`
1. Press CTRL-C to start tracing.
1. Press CTRL-C again to stop tracing.  The app should write data to file: `/tmp/YYYYMMDD_HH:MM:SS-trace.json`.
1. Press CTRL-C a third time to stop the process.

## Examine the trace data:
1. Open Chrome browser and navigate to its [trace browser tool](chrome://tracing).
1. Press the **Load** button and navigate to the TEF data file in `/tmp/`.
1. Be sure to make the Chrome window large so you can find the little navigation tool floater.
1. Select the navigation mode you want and then **left click-n-drag** the mouse to zoom/select/drag the trace graph.

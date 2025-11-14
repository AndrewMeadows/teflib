#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#define USE_TEF
#include "tracing.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

TRACE_GLOBAL_INIT


std::string filename = "/tmp/test_000.json";

void test_000() {

    constexpr uint8_t CTX_0 = 0;
    constexpr uint8_t CTX_1 = 1;
    constexpr uint8_t CTX_2 = 2;
    constexpr uint8_t CTX_3 = 3;

    constexpr uint8_t CAT_0 = 10;
    constexpr uint8_t CAT_1 = 11;
    constexpr uint8_t CAT_2 = 12;
    constexpr uint8_t CAT_3 = 13;

    constexpr uint8_t COUNT_0 = 20;
    constexpr uint8_t COUNT_1 = 21;
    constexpr uint8_t COUNT_2 = 22;
    constexpr uint8_t COUNT_3 = 23;

    constexpr uint8_t DATUM_0 = 30;
    constexpr uint8_t DATUM_1 = 31;
    constexpr uint8_t DATUM_2 = 32;
    constexpr uint8_t DATUM_3 = 33;

    // Start tracing to file for 5 seconds
    TRACE_REGISTER_STRING(CTX_0, "context_0");
    TRACE_REGISTER_STRING(CTX_1, "context_1");
    TRACE_REGISTER_STRING(CTX_2, "context_2");
    TRACE_REGISTER_STRING(CTX_3, "context_3");

    TRACE_REGISTER_STRING(CAT_0, "foo");
    TRACE_REGISTER_STRING(CAT_1, "foo,bar");
    TRACE_REGISTER_STRING(CAT_2, "bar,baz");
    TRACE_REGISTER_STRING(CAT_3, "foo,fubar");

    TRACE_REGISTER_STRING(COUNT_0, "count_0");
    TRACE_REGISTER_STRING(COUNT_1, "count_1");
    TRACE_REGISTER_STRING(COUNT_2, "count_2");
    TRACE_REGISTER_STRING(COUNT_3, "count_3");

    TRACE_REGISTER_STRING(DATUM_0, "datum_0");
    TRACE_REGISTER_STRING(DATUM_1, "datum_1");
    TRACE_REGISTER_STRING(DATUM_2, "datum_2");
    TRACE_REGISTER_STRING(DATUM_3, "datum_3");

    TRACE_START(5000, filename);
    {
        TRACE_CONTEXT(CTX_0, CAT_0);
        TRACE_COUNTER(COUNT_0, DATUM_0, 13);
        {
            TRACE_CONTEXT(CTX_1, CAT_1);
            TRACE_COUNTER(COUNT_1, DATUM_1, 17);
            {
                TRACE_CONTEXT(CTX_2, CAT_2);
                TRACE_COUNTER(COUNT_2, DATUM_2, 19);
            }
            {
                TRACE_CONTEXT(CTX_3, CAT_3);
                TRACE_COUNTER(COUNT_3, DATUM_3, 23);
            }
        }
    }
    TRACE_SHUTDOWN;

    // wrote to file /tmp/test_000.json
}

void validate_000() {
    // read from /tmp/test_000.json
    std::ifstream file(filename);
    assert(file.is_open() && "Failed to open trace file");

    json trace_data;
    file >> trace_data;
    file.close();

    // the contents of /tmp/test_000.json are expected to look something like this:
    /*
        {"traceEvents":[
        {"name":"count_0","ph":"C","ts":78,"pid":1,"tid":140140177810496,"args":{"datum_0":13}},
        {"name":"count_1","ph":"C","ts":80,"pid":1,"tid":140140177810496,"args":{"datum_1":17}},
        {"name":"count_2","ph":"C","ts":82,"pid":1,"tid":140140177810496,"args":{"datum_2":19}},
        {"name":"context_2","cat":"bar,baz","ph":"X","ts":81,"dur":2,"pid":1,"tid":140140177810496},
        {"name":"count_3","ph":"C","ts":85,"pid":1,"tid":140140177810496,"args":{"datum_3":23}},
        {"name":"context_3","cat":"foo,fubar","ph":"X","ts":84,"dur":2,"pid":1,"tid":140140177810496},
        {"name":"context_1","cat":"foo,bar","ph":"X","ts":79,"dur":8,"pid":1,"tid":140140177810496},
        {"name":"context_0","cat":"foo","ph":"X","ts":77,"dur":11,"pid":1,"tid":140140177810496},
        {"name":"end_of_trace","ph":"X","pid":1,"tid":140140177810496,"ts":104,"dur":1000}
        ]
        }
    */
    // The values of "tid", "ts", and "dur" fields will tend to change (they are timing measurements)
    // however the the rest of the values are expected to be invariant.

    // Validate structure
    assert(trace_data.contains("traceEvents"));
    auto& events = trace_data["traceEvents"];
    assert(events.is_array());
    assert(events.size() == 9);

    // Event 0: count_0
    assert(events[0]["name"] == "count_0");
    assert(events[0]["ph"] == "C");
    assert(events[0]["pid"] == 1);
    assert(events[0].contains("tid"));
    assert(events[0].contains("ts"));
    assert(events[0].contains("args"));
    assert(events[0]["args"]["datum_0"] == 13);

    // Event 1: count_1
    assert(events[1]["name"] == "count_1");
    assert(events[1]["ph"] == "C");
    assert(events[1]["pid"] == 1);
    assert(events[1].contains("tid"));
    assert(events[1].contains("ts"));
    assert(events[1].contains("args"));
    assert(events[1]["args"]["datum_1"] == 17);

    // Event 2: count_2
    assert(events[2]["name"] == "count_2");
    assert(events[2]["ph"] == "C");
    assert(events[2]["pid"] == 1);
    assert(events[2].contains("tid"));
    assert(events[2].contains("ts"));
    assert(events[2].contains("args"));
    assert(events[2]["args"]["datum_2"] == 19);

    // Event 3: context_2
    assert(events[3]["name"] == "context_2");
    assert(events[3]["cat"] == "bar,baz");
    assert(events[3]["ph"] == "X");
    assert(events[3]["pid"] == 1);
    assert(events[3].contains("tid"));
    assert(events[3].contains("ts"));
    assert(events[3].contains("dur"));

    // Event 4: count_3
    assert(events[4]["name"] == "count_3");
    assert(events[4]["ph"] == "C");
    assert(events[4]["pid"] == 1);
    assert(events[4].contains("tid"));
    assert(events[4].contains("ts"));
    assert(events[4].contains("args"));
    assert(events[4]["args"]["datum_3"] == 23);

    // Event 5: context_3
    assert(events[5]["name"] == "context_3");
    assert(events[5]["cat"] == "foo,fubar");
    assert(events[5]["ph"] == "X");
    assert(events[5]["pid"] == 1);
    assert(events[5].contains("tid"));
    assert(events[5].contains("ts"));
    assert(events[5].contains("dur"));

    // Event 6: context_1
    assert(events[6]["name"] == "context_1");
    assert(events[6]["cat"] == "foo,bar");
    assert(events[6]["ph"] == "X");
    assert(events[6]["pid"] == 1);
    assert(events[6].contains("tid"));
    assert(events[6].contains("ts"));
    assert(events[6].contains("dur"));

    // Event 7: context_0
    assert(events[7]["name"] == "context_0");
    assert(events[7]["cat"] == "foo");
    assert(events[7]["ph"] == "X");
    assert(events[7]["pid"] == 1);
    assert(events[7].contains("tid"));
    assert(events[7].contains("ts"));
    assert(events[7].contains("dur"));

    // Event 8: end_of_trace
    assert(events[8]["name"] == "end_of_trace");
    assert(events[8]["ph"] == "X");
    assert(events[8]["pid"] == 1);
    assert(events[8].contains("tid"));
    assert(events[8].contains("ts"));
    assert(events[8].contains("dur"));

    std::cout << "All validation tests passed!" << std::endl;
}

int main() {
    test_000();
    validate_000();
    return 0;
}

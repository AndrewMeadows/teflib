// timing_util.cpp
//

#include "timing_util.h"

#include <fmt/format.h>


// YYYYMMDD_HH:MM:SS
std::string timing_util::get_local_datetime_string(uint64_t now_msec) {
    std::string s;
    s.resize(17); // YYYYMMDD_HH:MM:SS
    const std::time_t now_sec_std =
        (std::time_t)((int64_t)(now_msec) / MSEC_PER_SECOND);
    auto t = std::localtime(&now_sec_std);
    std::strftime((char*)(s.data()), s.size(), "%Y%m%d_", t);
    std::strftime((char*)(s.data()+9), s.size(), "%T", t);
    return s;
}
std::string timing_util::get_local_datetime_string() {
    uint64_t now = timing_util::get_now_msec();
    return timing_util::get_local_datetime_string(now);
}

// YYYYMMDD_HH:MM:SS.msec
std::string timing_util::get_local_datetime_string_with_msec(uint64_t now_msec) {
    std::string s;
    s.resize(17); // YYYYMMDD_HH:MM:SS
    const std::time_t now_sec_std =
        (std::time_t)((int64_t)(now_msec) / MSEC_PER_SECOND);
    auto t = std::localtime(&now_sec_std);
    std::strftime((char*)(s.data()), s.size(), "%Y%m%d_", t);
    std::strftime((char*)(s.data()+9), s.size(), "%T", t);
    s.append(fmt::format(".{:0<3}", now_msec % MSEC_PER_SECOND));
    return s;
}

std::string timing_util::get_local_datetime_string_with_msec() {
    uint64_t now = timing_util::get_now_msec();
    return timing_util::get_local_datetime_string_with_msec(now);
}

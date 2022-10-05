// log_util.cpp
//

#include "log_util.h"


namespace log_util {

uint32_t g_log_verbosity = 0;

void set_verbosity(uint32_t verbosity) {
    g_log_verbosity = verbosity;
}

uint32_t get_verbosity() {
    return g_log_verbosity;
}

} // namespace log_util

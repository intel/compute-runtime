/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_main_helpers.h"

#include "shared/test/common/helpers/custom_event_listener.h"
#include "shared/test/common/helpers/debug_variables_listener.h"
#include "shared/test/common/helpers/memory_leak_listener.h"
#include "shared/test/common/helpers/virtual_file_system_listener.h"
#include "shared/test/common/test_stats.h"

#include "gtest/gtest.h"

#include <cstring>
#include <fstream>
#include <iostream>

namespace NEO {

bool parseCommonUltOption(int argc, char **argv, int &argIndex, UltCommandLineOptions &options, bool &enableAlarm) {
    const char *arg = argv[argIndex];
    if (!strcmp("--disable_default_listener", arg)) {
        options.useDefaultListener = false;
    } else if (!strcmp("--enable_default_listener", arg)) {
        options.useDefaultListener = true;
    } else if (!strcmp("--disable_alarm", arg)) {
        enableAlarm = false;
    } else if (!strcmp("--show_test_stats", arg)) {
        options.showTestStats = true;
    } else if (!strcmp("--dump_test_stats", arg) && argIndex + 1 < argc) {
        options.dumpTestStatsFileName = argv[++argIndex];
    } else {
        return false;
    }
    return true;
}

CCustomEventListener *replaceDefaultResultPrinter(::testing::TestEventListeners &listeners) {
    auto defaultListener = listeners.default_result_printer();
    auto customEventListener = new CCustomEventListener(defaultListener);
    listeners.Release(defaultListener);
    listeners.Append(customEventListener);
    return customEventListener;
}

void appendCommonUltListeners(::testing::TestEventListeners &listeners) {
    listeners.Append(new MemoryLeakListener);
    listeners.Append(new DebugVariablesListener);
    listeners.Append(new VirtualFileSystemListener);
}

void reportTestStats(const UltCommandLineOptions &options) {
    if (options.showTestStats) {
        std::cout << getTestStats() << std::endl;
    }
    if (!options.dumpTestStatsFileName.empty()) {
        std::ofstream dumpTestStatsFile(options.dumpTestStatsFileName);
        dumpTestStatsFile << getTestStatsJson();
    }
}

} // namespace NEO

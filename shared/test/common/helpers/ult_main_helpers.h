/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace testing {
class TestEventListeners;
} // namespace testing

class CCustomEventListener;

namespace NEO {

struct UltCommandLineOptions {
    bool useDefaultListener = false;
    bool allowOutput = false;
    bool showTestStats = false;
    std::string dumpTestStatsFileName;
};

bool parseCommonUltOption(int argc, char **argv, int &argIndex, UltCommandLineOptions &options, bool &enableAlarm);
CCustomEventListener *replaceDefaultResultPrinter(::testing::TestEventListeners &listeners);
void appendCommonUltListeners(::testing::TestEventListeners &listeners);
void reportTestStats(const UltCommandLineOptions &options);

} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/source/debug_settings/definitions/translate_debug_settings.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include <iostream>
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

template <DebugFunctionalityLevel debugLevel>
void readDebugSettingsFromConfig(DebugSettingsManager<debugLevel> &settingsManager) {
    if (!DebugSettingsManager<debugLevel>::registryReadAvailable() && !isDebugKeysReadEnabled()) {
        std::cout << "--read-config: debug variables are not supported in this build type, only release variables are read" << std::endl;
    }
    settingsManager.setReaderImpl(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    settingsManager.injectSettingsFromReader();
    translateDebugSettings(settingsManager.flags);
}

} // namespace NEO

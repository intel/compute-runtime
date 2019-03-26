/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/debug_settings_reader_creator.h"

namespace NEO {

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager() {
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() = default;

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
    std::ofstream outFile(filename, mode);
    if (outFile.is_open()) {
        outFile.write(str, length);
        outFile.close();
    }
}

// Global Debug Settings Manager
DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager;

// Global table of hardware prefixes
const char *hardwarePrefix[IGFX_MAX_PRODUCT] = {
    nullptr,
};

// Global table of family names
const char *familyName[IGFX_MAX_CORE] = {
    nullptr,
};

// Global table of family names
bool familyEnabled[IGFX_MAX_CORE] = {
    false,
};

} // namespace NEO

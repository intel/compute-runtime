/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"

namespace NEO {

template <DebugFunctionalityLevel debugLevel>
DebugSettingsManager<debugLevel>::DebugSettingsManager(const char *registryPath) {
}

template <DebugFunctionalityLevel debugLevel>
DebugSettingsManager<debugLevel>::~DebugSettingsManager() {
    readerImpl.reset();
};

// Global Debug Settings Manager
DebugSettingsManager<globalDebugFunctionalityLevel> debugManager("");
} // namespace NEO

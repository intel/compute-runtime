/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"

namespace NEO {

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager(const char *registryPath) {
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() = default;

// Global Debug Settings Manager
DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager("");
} // namespace NEO

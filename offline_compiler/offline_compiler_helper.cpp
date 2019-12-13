/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/hw_info.h"
#include "core/utilities/debug_settings_reader_creator.h"

namespace NEO {

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager(const char *registryPath) {
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() = default;

// Global Debug Settings Manager
DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager("");
} // namespace NEO

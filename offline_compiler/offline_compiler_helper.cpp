/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"
#include "core/utilities/debug_settings_reader_creator.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace NEO {

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager() {
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() = default;

// Global Debug Settings Manager
DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager;
} // namespace NEO

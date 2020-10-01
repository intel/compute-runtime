/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"

namespace NEO {
bool ApiSpecificConfig::getHeapConfiguration() {
    return DebugManager.flags.UseExternalAllocatorForSshAndDsh.get();
}
bool ApiSpecificConfig::getBindelssConfiguration() {
    if (DebugManager.flags.UseBindlessBuiltins.get() != -1) {
        return DebugManager.flags.UseBindlessBuiltins.get();
    } else {
        return false;
    }
}
} // namespace NEO
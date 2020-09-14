/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/heap_assigner_config.h"

namespace NEO {
bool HeapAssignerConfig::getConfiguration() {
    return DebugManager.flags.UseExternalAllocatorForSshAndDsh.get();
}
} // namespace NEO
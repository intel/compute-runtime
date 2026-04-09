/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debug_settings/definitions/translate_debug_settings.h"
#include "shared/source/helpers/flush_caches_bitmask.h"

namespace NEO {

void translateDebugSettings(DebugVariables &debugVariables) {
    translateDebugSettingsImpl(debugVariables);

    if (debugVariables.FlushAllCaches.get() == 1) {
        debugVariables.FlushAllCaches.set(FlushCachesBitmask::allCaches);
    }
}

} // namespace NEO

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

L0HwHelper *l0HwHelperFactory[IGFX_MAX_CORE] = {};

L0HwHelper &L0HwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *l0HwHelperFactory[gfxCore];
}

bool L0HwHelper::enableFrontEndStateTracking() {
    constexpr bool defaultValue = false;
    if (NEO::DebugManager.flags.EnableFrontEndTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableFrontEndTracking.get();
    }
    return defaultValue;
}

bool L0HwHelper::enablePipelineSelectStateTracking() {
    constexpr bool defaultValue = false;
    if (NEO::DebugManager.flags.EnablePipelineSelectTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnablePipelineSelectTracking.get();
    }
    return defaultValue;
}

bool L0HwHelper::enableStateComputeModeTracking() {
    constexpr bool defaultValue = false;
    if (NEO::DebugManager.flags.EnableStateComputeModeTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableStateComputeModeTracking.get();
    }
    return defaultValue;
}

bool L0HwHelper::enableImmediateCmdListHeapSharing(bool cmdlistSupport) {
    bool enabled = false;
    if (NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get() != -1) {
        return !!NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get();
    }
    return enabled;
}

} // namespace L0

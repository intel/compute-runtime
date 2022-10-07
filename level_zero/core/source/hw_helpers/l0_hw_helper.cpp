/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include "shared/source/helpers/hw_info.h"

namespace L0 {

L0HwHelper *l0HwHelperFactory[IGFX_MAX_CORE] = {};

L0HwHelper &L0HwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *l0HwHelperFactory[gfxCore];
}

bool L0HwHelper::enableFrontEndStateTracking(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.EnableFrontEndTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableFrontEndTracking.get();
    }
    return get(hwInfo.platform.eRenderCoreFamily).platformSupportsFrontEndTracking(hwInfo);
}

bool L0HwHelper::enablePipelineSelectStateTracking(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.EnablePipelineSelectTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnablePipelineSelectTracking.get();
    }
    return get(hwInfo.platform.eRenderCoreFamily).platformSupportsPipelineSelectTracking(hwInfo);
}

bool L0HwHelper::enableStateComputeModeTracking(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.EnableStateComputeModeTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableStateComputeModeTracking.get();
    }
    return get(hwInfo.platform.eRenderCoreFamily).platformSupportsStateComputeModeTracking(hwInfo);
}

bool L0HwHelper::enableImmediateCmdListHeapSharing(const NEO::HardwareInfo &hwInfo, bool cmdlistSupport) {
    if (NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get() != -1) {
        return !!NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get();
    }
    bool platformSupport = get(hwInfo.platform.eRenderCoreFamily).platformSupportsCmdListHeapSharing(hwInfo);
    return platformSupport && cmdlistSupport;
}

} // namespace L0

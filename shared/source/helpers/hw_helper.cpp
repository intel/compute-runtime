/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"

#include <algorithm>

namespace NEO {
GfxCoreHelper *gfxCoreHelperFactory[IGFX_MAX_CORE] = {};

GfxCoreHelper &GfxCoreHelper::get(GFXCORE_FAMILY gfxCore) {
    return *gfxCoreHelperFactory[gfxCore];
}

bool GfxCoreHelper::compressedBuffersSupported(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!DebugManager.flags.RenderCompressedBuffersEnabled.get();
    }
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers;
}

bool GfxCoreHelper::compressedImagesSupported(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!DebugManager.flags.RenderCompressedImagesEnabled.get();
    }
    return hwInfo.capabilityTable.ftrRenderCompressedImages;
}

bool GfxCoreHelper::cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo) {
    int32_t dbgFlag = DebugManager.flags.EnableCacheFlushAfterWalker.get();
    if (dbgFlag == 1) {
        return true;
    } else if (dbgFlag == 0) {
        return false;
    }
    return hwInfo.capabilityTable.supportCacheFlushAfterWalker;
}

uint32_t GfxCoreHelper::getMaxThreadsForVfe(const HardwareInfo &hwInfo) {
    uint32_t threadsPerEU = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    auto maxHwThreadsCapable = hwInfo.gtSystemInfo.EUCount * threadsPerEU;
    auto maxHwThreadsReturned = maxHwThreadsCapable;
    if (DebugManager.flags.MaxHwThreadsPercent.get() != 0) {
        maxHwThreadsReturned = int(maxHwThreadsCapable * (DebugManager.flags.MaxHwThreadsPercent.get() / 100.0f));
    }
    if (DebugManager.flags.MinHwThreadsUnoccupied.get() != 0) {
        maxHwThreadsReturned = std::min(maxHwThreadsReturned, maxHwThreadsCapable - DebugManager.flags.MinHwThreadsUnoccupied.get());
    }
    return maxHwThreadsReturned;
}

uint32_t GfxCoreHelper::getSubDevicesCount(const HardwareInfo *pHwInfo) {
    if (DebugManager.flags.CreateMultipleSubDevices.get() > 0) {
        return DebugManager.flags.CreateMultipleSubDevices.get();
    } else if (pHwInfo->gtSystemInfo.MultiTileArchInfo.IsValid && pHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount > 0u) {
        return pHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount;
    } else {
        return 1u;
    }
}

uint32_t GfxCoreHelper::getHighestEnabledSlice(const HardwareInfo &hwInfo) {
    uint32_t highestEnabledSlice = 0;
    if (!hwInfo.gtSystemInfo.IsDynamicallyPopulated) {
        return hwInfo.gtSystemInfo.MaxSlicesSupported;
    }
    for (int highestSlice = GT_MAX_SLICE - 1; highestSlice >= 0; highestSlice--) {
        if (hwInfo.gtSystemInfo.SliceInfo[highestSlice].Enabled) {
            highestEnabledSlice = highestSlice + 1;
            break;
        }
    }
    return highestEnabledSlice;
}

void HwHelper::getCpuCopyThresholds(size_t &h2DThreshold, size_t &d2HThreshold) {
    h2DThreshold = NonUsmCpuCopyConstants::h2DThreshold;
    d2HThreshold = NonUsmCpuCopyConstants::d2HThreshold;

    if (NEO::DebugManager.flags.ExperimentalH2DCpuCopyThreshold.get() != -1) {
        h2DThreshold = NEO::DebugManager.flags.ExperimentalH2DCpuCopyThreshold.get();
    }
    if (NEO::DebugManager.flags.ExperimentalD2HCpuCopyThreshold.get() != -1) {
        d2HThreshold = NEO::DebugManager.flags.ExperimentalD2HCpuCopyThreshold.get();
    }
}

} // namespace NEO

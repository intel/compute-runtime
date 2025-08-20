/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"

#include <algorithm>

namespace NEO {

GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[IGFX_MAX_CORE] = {};

std::unique_ptr<GfxCoreHelper> GfxCoreHelper::create(const GFXCORE_FAMILY gfxCoreFamily) {

    auto createFunction = gfxCoreHelperFactory[gfxCoreFamily];
    if (createFunction == nullptr) {
        return nullptr;
    }
    auto gfxCoreHelper = createFunction();
    return gfxCoreHelper;
}

bool GfxCoreHelper::compressedBuffersSupported(const HardwareInfo &hwInfo) {
    if (debugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!debugManager.flags.RenderCompressedBuffersEnabled.get();
    }
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers;
}

bool GfxCoreHelper::compressedImagesSupported(const HardwareInfo &hwInfo) {
    if (debugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!debugManager.flags.RenderCompressedImagesEnabled.get();
    }
    return hwInfo.capabilityTable.ftrRenderCompressedImages;
}

bool GfxCoreHelper::cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo) {
    int32_t dbgFlag = debugManager.flags.EnableCacheFlushAfterWalker.get();
    if (dbgFlag == 1) {
        return true;
    } else if (dbgFlag == 0) {
        return false;
    }
    return hwInfo.capabilityTable.supportCacheFlushAfterWalker;
}

uint32_t GfxCoreHelper::getMaxThreadsForVfe(const HardwareInfo &hwInfo) {
    uint32_t threadsPerEU = hwInfo.gtSystemInfo.NumThreadsPerEu + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    auto maxHwThreadsCapable = hwInfo.gtSystemInfo.EUCount * threadsPerEU;
    auto maxHwThreadsReturned = maxHwThreadsCapable;
    if (debugManager.flags.MaxHwThreadsPercent.get() != 0) {
        maxHwThreadsReturned = int(maxHwThreadsCapable * (debugManager.flags.MaxHwThreadsPercent.get() / 100.0f));
    }
    if (debugManager.flags.MinHwThreadsUnoccupied.get() != 0) {
        maxHwThreadsReturned = std::min(maxHwThreadsReturned, maxHwThreadsCapable - debugManager.flags.MinHwThreadsUnoccupied.get());
    }
    return maxHwThreadsReturned;
}

uint32_t GfxCoreHelper::getSubDevicesCount(const HardwareInfo *pHwInfo) {
    if (debugManager.flags.CreateMultipleSubDevices.get() > 0) {
        return debugManager.flags.CreateMultipleSubDevices.get();
    } else if (pHwInfo->gtSystemInfo.MultiTileArchInfo.IsValid && pHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount > 0u) {
        return pHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount;
    } else {
        return 1u;
    }
}

uint32_t GfxCoreHelper::getHighestEnabledSlice(const HardwareInfo &hwInfo) {
    uint32_t highestEnabledSlice = 1;
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

uint32_t getHighestEnabledSubSlice(const HardwareInfo &hwInfo) {
    uint32_t numSubSlicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t highestEnabledSliceIdx = GfxCoreHelper::getHighestEnabledSlice(hwInfo) - 1;
    uint32_t highestSubSlice = (highestEnabledSliceIdx + 1) * numSubSlicesPerSlice;

    for (int32_t subSliceId = GT_MAX_SUBSLICE_PER_SLICE - 1; subSliceId >= 0; subSliceId--) {
        if (hwInfo.gtSystemInfo.SliceInfo[highestEnabledSliceIdx].SubSliceInfo[subSliceId].Enabled) {
            return highestEnabledSliceIdx * numSubSlicesPerSlice + subSliceId + 1;
        }
    }

    return highestSubSlice;
}

uint32_t GfxCoreHelper::getHighestEnabledDualSubSlice(const HardwareInfo &hwInfo) {
    uint32_t maxDualSubSlicesSupported = hwInfo.gtSystemInfo.MaxDualSubSlicesSupported;
    if (!hwInfo.gtSystemInfo.IsDynamicallyPopulated) {
        return maxDualSubSlicesSupported;
    }

    if (maxDualSubSlicesSupported == 0) {
        return getHighestEnabledSubSlice(hwInfo);
    }

    uint32_t numDssPerSlice = maxDualSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t highestEnabledSliceIdx = getHighestEnabledSlice(hwInfo) - 1;
    uint32_t highestDualSubSlice = (highestEnabledSliceIdx + 1) * numDssPerSlice;

    for (int32_t dssID = GT_MAX_DUALSUBSLICE_PER_SLICE - 1; dssID >= 0; dssID--) {
        if (hwInfo.gtSystemInfo.SliceInfo[highestEnabledSliceIdx].DSSInfo[dssID].Enabled) {
            return (highestEnabledSliceIdx * numDssPerSlice) + dssID + 1;
        }
    }

    return highestDualSubSlice;
}

bool GfxCoreHelper::isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo, const ProductHelper &productHelper) {
    auto lowestHwRevIdWithBug = productHelper.getHwRevIdFromStepping(lowestSteppingWithBug, hwInfo);
    auto hwRevIdWithFix = productHelper.getHwRevIdFromStepping(steppingWithFix, hwInfo);
    if ((lowestHwRevIdWithBug == CommonConstants::invalidStepping) || (hwRevIdWithFix == CommonConstants::invalidStepping)) {
        return false;
    }
    return (lowestHwRevIdWithBug <= hwInfo.platform.usRevId && hwInfo.platform.usRevId < hwRevIdWithFix);
}

} // namespace NEO

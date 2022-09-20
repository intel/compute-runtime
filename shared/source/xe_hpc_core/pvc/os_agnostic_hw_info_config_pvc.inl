/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return PVC::isXl(hwInfo);
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    if (PVC::isXt(hwInfo)) {
        switch (stepping) {
        case REVISION_A0:
            return 0x3;
        case REVISION_B:
            return 0x9D;
        case REVISION_C:
            return 0x7;
        }
    } else {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x6;
        case REVISION_C:
            DEBUG_BREAK_IF(true);
            return 0x7;
        }
    }

    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId & PVC::pvcSteppingBits) {
    case 0x0:
    case 0x1:
    case 0x3:
        return REVISION_A0;
    case 0x5:
    case 0x6:
        return REVISION_B;
    case 0x7:
        return REVISION_C;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    uint32_t stepping = getSteppingFromHwRevId(hwInfo);
    if (stepping == CommonConstants::invalidStepping) {
        return AOT::UNKNOWN_ISA;
    }

    if (PVC::isXl(hwInfo)) {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::PVC_XL_A0;
        default:
        case 0x1:
            return AOT::PVC_XL_A0P;
        }
    } else {
        switch (hwInfo.platform.usRevId) {
        case 0x3:
            return AOT::PVC_XT_A0;
        case 05:
            return AOT::PVC_XT_B0;
        case 06:
            return AOT::PVC_XT_B1;
        default:
        case 07:
            return AOT::PVC_XT_C0;
        }
    }
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return PVC::isAtMostXtA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return !PVC::isXlA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const {
    return PVC::isXlA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

bool isBaseDieA0(const HardwareInfo &hwInfo) {
    return (hwInfo.platform.usRevId & PVC::pvcBaseDieRevMask) == PVC::pvcBaseDieA0Masked;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const {
    bool baseDieA0 = isBaseDieA0(hwInfo);
    bool applyWa = ((DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() == 1) || baseDieA0);
    applyWa &= (DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() != 0);
    return applyWa;
}

template <>
bool HwInfoConfigHw<gfxProduct>::allowMemoryPrefetch(const HardwareInfo &hwInfo) const {
    bool prefetch = !isBaseDieA0(hwInfo);
    if (DebugManager.flags.EnableMemoryPrefetch.get() != -1) {
        prefetch = !!DebugManager.flags.EnableMemoryPrefetch.get();
    }
    return prefetch;
}
template <>
bool HwInfoConfigHw<gfxProduct>::isBcsReportWaRequired(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.DoNotReportTile1BscWaActive.get() != -1) {
        return DebugManager.flags.DoNotReportTile1BscWaActive.get();
    }
    return isBaseDieA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const {
    if (!allocation.isAllocatedInLocalMemoryPool()) {
        return false;
    }

    if (getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed) {
        // Regular L3 WA
        return true;
    }

    if (!allocation.isAllocationLockable()) {
        return true;
    }

    bool isDieA0 = isBaseDieA0(hwInfo);
    bool isOtherTileThan0Accessed = allocation.storageInfo.memoryBanks.to_ulong() > 1u;
    if (isDieA0 && isOtherTileThan0Accessed) {
        // Tile1 CPU access
        return true;
    }

    return false;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isBlitSplitEnqueueWARequired(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isImplicitScalingSupported(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isAssignEngineRoundRobinSupported() const {
    return false;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return PVC::isXlA0(hwInfo) ? 8u : 16u;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return PVC::isXt(hwInfo);
}

template <>
void HwInfoConfigHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isStatefulAddressingModeSupported() const {
    return false;
}
/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/definitions/indirect_detection_versions.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_before_xe2.inl"
#include "shared/source/os_interface/product_helper_xe_hpg_and_later.inl"

#include "aubstream/product_family.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return PVC::isXl(hwInfo);
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    if (PVC::isXtVg(hwInfo)) {
        if (stepping == REVISION_C) {
            return 0x7;
        }
    } else if (PVC::isXt(hwInfo)) {
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
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    auto rev = hwInfo.platform.usRevId & PVC::pvcSteppingBits;

    if (PVC::isXtVg(hwInfo) && rev != 7) {
        return CommonConstants::invalidStepping;
    }

    switch (rev) {
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
bool ProductHelperHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool ProductHelperHw<gfxProduct>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return PVC::isAtMostXtA0(hwInfo);
}

template <>
bool ProductHelperHw<gfxProduct>::isReleaseGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return !PVC::isXlA0(hwInfo);
}

template <>
bool ProductHelperHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

bool isBaseDieA0(const HardwareInfo &hwInfo) {
    return (hwInfo.platform.usRevId & PVC::pvcBaseDieRevMask) == PVC::pvcBaseDieA0Masked;
}

template <>
bool ProductHelperHw<gfxProduct>::isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const {
    bool baseDieA0 = isBaseDieA0(hwInfo);
    bool applyWa = ((debugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() == 1) || baseDieA0);
    applyWa &= (debugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() != 0);
    return applyWa;
}

template <>
bool ProductHelperHw<gfxProduct>::allowMemoryPrefetch(const HardwareInfo &hwInfo) const {
    bool prefetch = !isBaseDieA0(hwInfo);
    if (debugManager.flags.EnableMemoryPrefetch.get() != -1) {
        prefetch = !!debugManager.flags.EnableMemoryPrefetch.get();
    }
    return prefetch;
}
template <>
bool ProductHelperHw<gfxProduct>::isBcsReportWaRequired(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.DoNotReportTile1BscWaActive.get() != -1) {
        return debugManager.flags.DoNotReportTile1BscWaActive.get();
    }
    return isBaseDieA0(hwInfo);
}

template <>
bool ProductHelperHw<gfxProduct>::isBlitCopyRequiredForLocalMemory(const RootDeviceEnvironment &rootDeviceEnvironment, const GraphicsAllocation &allocation) const {
    if (!allocation.isAllocatedInLocalMemoryPool()) {
        return false;
    }

    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::cpuAccessDisallowed) {
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
void ProductHelperHw<gfxProduct>::parseCcsMode(std::string ccsModeString, std::unordered_map<uint32_t, uint32_t> &rootDeviceNumCcsMap, uint32_t rootDeviceIndex, RootDeviceEnvironment *rootDeviceEnvironment) const {
    auto numberOfCcsEntries = StringHelpers::split(ccsModeString, ",");

    for (const auto &entry : numberOfCcsEntries) {
        auto subEntries = StringHelpers::split(entry, ":");
        uint32_t rootDeviceIndexParsed = StringHelpers::toUint32t(subEntries[0]);

        if (rootDeviceIndexParsed == rootDeviceIndex) {
            if (subEntries.size() > 1) {
                uint32_t maxCcsCount = StringHelpers::toUint32t(subEntries[1]);
                rootDeviceNumCcsMap.insert({rootDeviceIndex, maxCcsCount});
                rootDeviceEnvironment->setNumberOfCcs(maxCcsCount);
            }
        }
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    bool tlbFlushRequired = false;
    if (debugManager.flags.ForceTlbFlush.get() != -1) {
        tlbFlushRequired = !!debugManager.flags.ForceTlbFlush.get();
    }
    return tlbFlushRequired;
}

template <>
BcsSplitSettings ProductHelperHw<gfxProduct>::getBcsSplitSettings() const {
    constexpr BcsInfoMask oddLinkedCopyEnginesMask = NEO::EngineHelpers::oddLinkedCopyEnginesMask;

    return {
        .allEngines = oddLinkedCopyEnginesMask,
        .h2dEngines = NEO::EngineHelpers::h2dCopyEngineMask,
        .d2hEngines = NEO::EngineHelpers::d2hCopyEngineMask,
        .minRequiredTotalCsrCount = static_cast<uint32_t>(oddLinkedCopyEnginesMask.count()),
        .requiredTileCount = 1,
        .enabled = true,
    };
}

template <>
bool ProductHelperHw<gfxProduct>::isInitDeviceWithFirstSubmissionRequired(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isImplicitScalingSupported(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool ProductHelperHw<gfxProduct>::isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return PVC::isXlA0(hwInfo) ? 8u : 16u;
}

template <>
bool ProductHelperHw<gfxProduct>::isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool ProductHelperHw<gfxProduct>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return PVC::isXt(hwInfo) || PVC::isXtVg(hwInfo);
}

template <>
void ProductHelperHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
}

template <>
bool ProductHelperHw<gfxProduct>::isStatefulAddressingModeSupported() const {
    return false;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getRequiredDetectIndirectVersion() const {
    return IndirectDetectionVersions::requiredDetectIndirectVersionPVC;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getRequiredDetectIndirectVersionVC() const {
    return IndirectDetectionVersions::requiredDetectIndirectVersionPVCVectorCompiler;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Pvc;
};

template <>
uint32_t ProductHelperHw<gfxProduct>::getNumberOfPartsInTileForConcurrentKernel(uint32_t ccsCount) const {
    if (ccsCount == 1) {
        return 1;
    } else if (ccsCount == 2) {
        return 4;
    }
    return PVC::numberOfpartsInTileForConcurrentKernels;
}

template <>
bool ProductHelperHw<gfxProduct>::isSkippingStatefulInformationRequired(const KernelDescriptor &kernelDescriptor) const {
    bool isGeneratedByNgen = !kernelDescriptor.kernelMetadata.isGeneratedByIgc;
    return isGeneratedByNgen;
}

template <>
bool ProductHelperHw<gfxProduct>::supportReadOnlyAllocations() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isSharingWith3dOrMediaAllowed() const {
    return false;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return (UnifiedSharedMemoryFlags::access);
}

template <>
bool ProductHelperHw<gfxProduct>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    if (isMaxThreadsForWorkgroupWARequired(hwInfo)) {
        return std::min(getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice), 64u);
    }
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}

} // namespace NEO

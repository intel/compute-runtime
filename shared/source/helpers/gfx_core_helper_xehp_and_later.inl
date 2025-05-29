/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "aubstream/engine_node.h"

namespace NEO {

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (debugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(debugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto maxSubSlice = productHelper.computeMaxNeededSubSliceSpace(*hwInfo);
    // XeHP and later products return physical threads
    return maxSubSlice * hwInfo->gtSystemInfo.MaxEuPerSubSlice * (hwInfo->gtSystemInfo.ThreadCount / hwInfo->gtSystemInfo.EUCount);
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::getGlobalTimeStampBits() const {
    return 32;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrLocalMemory;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::hvAlign4Required() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::timestampPacketWriteSupported() const {
    return true;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getAmountOfAllocationsToFill() const {
    if (debugManager.flags.SetAmountOfReusableAllocations.get() != -1) {
        return debugManager.flags.SetAmountOfReusableAllocations.get();
    }
    return 1u;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::makeResidentBeforeLockNeeded(bool precondition) const {
    return true;
}

template <typename GfxFamily>
EngineGroupType GfxCoreHelperHw<GfxFamily>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    if (engineType == aub_stream::ENGINE_RCS) {
        return EngineGroupType::renderCompute;
    }
    if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
        return EngineGroupType::compute;
    }
    if (engineType == aub_stream::ENGINE_BCS) {
        return EngineGroupType::copy;
    }
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        if (debugManager.flags.ForceL1Caching.get() != 1) {
            l1enabled = static_cast<bool>(debugManager.flags.ForceL1Caching.get());
        }
        if (l1enabled) {
            return gmmHelper.getL1EnabledMOCS() >> 1;
        }
        return gmmHelper.getL3EnabledMOCS() >> 1;
    }

    return gmmHelper.getUncachedMOCS() >> 1;
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::calculateMaxWorkGroupSize(const KernelDescriptor &kernelDescriptor, uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (kernelDescriptor.kernelAttributes.simdSize != 32 && kernelDescriptor.kernelAttributes.numGrfRequired == GrfConfig::largeGrfNumber) {
        defaultMaxGroupSize >>= 1;
    }
    defaultMaxGroupSize = adjustMaxWorkGroupSize(kernelDescriptor.kernelAttributes.numGrfRequired, kernelDescriptor.kernelAttributes.simdSize, defaultMaxGroupSize, rootDeviceEnvironment);
    return std::min(defaultMaxGroupSize, CommonConstants::maxWorkgroupSize);
}

constexpr uint32_t planarYuvMaxHeight = 16128;

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getPlanarYuvMaxHeight() const {
    return planarYuvMaxHeight;
}

template <typename GfxFamily>
aub_stream::MMIOList GfxCoreHelperHw<GfxFamily>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    aub_stream::MMIOList mmioList;

    if (debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        auto format = static_cast<uint32_t>(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());

        UNRECOVERABLE_IF(format > 0x1F);

        uint32_t value = 1;     // [0] enable
        value |= (format << 3); // [3:7] compression_format

        mmioList.push_back({0x519C, value});
        mmioList.push_back({0xB0F0, value});
        mmioList.push_back({0xE4C0, value});
    }

    return mmioList;
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return hwInfo.featureTable.flags.ftrLocalMemory;
    }
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isScratchSpaceSurfaceStateAccessible() const {
    return true;
}
template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMaxScratchSize(const NEO::ProductHelper &productHelper) const {
    return 256 * MemoryConstants::kiloByte;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::platformSupportsImplicitScaling(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return ImplicitScalingDispatch<GfxFamily>::platformSupportsImplicitScaling(rootDeviceEnvironment);
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::preferInternalBcsEngine() const {
    auto preferInternalBcsEngine = true;
    if (debugManager.flags.PreferInternalBcsEngine.get() != -1) {
        preferInternalBcsEngine = static_cast<bool>(debugManager.flags.PreferInternalBcsEngine.get());
    }

    return preferInternalBcsEngine;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMinimalScratchSpaceSize() const {
    return 64U;
}

template <>
bool GfxCoreHelperHw<Family>::isChipsetUniqueUUIDSupported() const {
    return true;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getKernelPrivateMemSize(const KernelDescriptor &kernelDescriptor) const {
    const auto &kernelAttributes = kernelDescriptor.kernelAttributes;
    return (kernelAttributes.privateScratchMemorySize > 0) ? kernelAttributes.privateScratchMemorySize : kernelAttributes.perHwThreadPrivateMemorySize;
}
} // namespace NEO

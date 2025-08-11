/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/aub_mapper.h"
#include "shared/source/gen12lp/hw_cmds_base.h"

using Family = NEO::Gen12LpFamily;

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_to_dg2.inl"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) {
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::getGlobalTimeStampBits() const {
    return 36;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::hvAlign4Required() const {
    return true;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::timestampPacketWriteSupported() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isUpdateTaskCountFromWaitSupported() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getAmountOfAllocationsToFill() const {
    if (debugManager.flags.SetAmountOfReusableAllocations.get() != -1) {
        return debugManager.flags.SetAmountOfReusableAllocations.get();
    }
    return 0u;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::makeResidentBeforeLockNeeded(bool precondition) const {
    return precondition;
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::calculateMaxWorkGroupSize(const KernelDescriptor &kernelDescriptor, uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return std::min(defaultMaxGroupSize, CommonConstants::maxWorkgroupSize);
}

constexpr uint32_t planarYuvMaxHeight = 16352;

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getPlanarYuvMaxHeight() const {
    return planarYuvMaxHeight;
}

template <typename GfxFamily>
aub_stream::MMIOList GfxCoreHelperHw<GfxFamily>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    return {};
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(PipeControlArgs &args) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setBarrierWaFlags(void *barrierCmd) {
    reinterpret_cast<typename GfxFamily::PIPE_CONTROL *>(barrierCmd)->setCommandStreamerStallEnable(true);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::unTypedDataPortCacheFlushRequired() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isScratchSpaceSurfaceStateAccessible() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMaxScratchSize(const NEO::ProductHelper &productHelper) const {
    return 2 * MemoryConstants::megaByte;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::platformSupportsImplicitScaling(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return false;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::preferInternalBcsEngine() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMinimalScratchSpaceSize() const {
    return 1024U;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getKernelPrivateMemSize(const KernelDescriptor &kernelDescriptor) const {
    return kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize;
}

template <>
inline bool GfxCoreHelperHw<Family>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const {
    auto fusedEuDispatchEnabled = !hwInfo.workaroundTable.flags.waDisableFusedThreadScheduling;
    fusedEuDispatchEnabled &= hwInfo.capabilityTable.fusedEuEnabled;

    if (disableEUFusionForKernel)
        fusedEuDispatchEnabled = false;

    if (debugManager.flags.CFEFusedEUDispatch.get() != -1) {
        fusedEuDispatchEnabled = (debugManager.flags.CFEFusedEUDispatch.get() == 0);
    }
    return fusedEuDispatchEnabled;
}

template <>
size_t GfxCoreHelperHw<Family>::getMax3dImageWidthOrHeight() const {
    return 2048;
}

template <>
bool GfxCoreHelperHw<Family>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    return GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, productHelper);
}

template <>
bool GfxCoreHelperHw<Family>::isWaDisableRccRhwoOptimizationRequired() const {
    return true;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    /* For ICL+ maxThreadCount equals (EUCount * 8).
     ThreadCount/EUCount=7 is no longer valid, so we have to force 8 in below formula.
     This is required to allocate enough scratch space. */
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    return hwInfo->gtSystemInfo.MaxSubSlicesSupported * hwInfo->gtSystemInfo.MaxEuPerSubSlice * 8;
}

template <>
bool GfxCoreHelperHw<Family>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrLocalMemory;
}

template <>
bool GfxCoreHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size) const {
    if (debugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!debugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }
    return false;
}

template <>
bool GfxCoreHelperHw<Family>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) const {
    return !graphicsAllocation.isCompressionEnabled();
}

template <>
uint32_t GfxCoreHelperHw<Family>::getPitchAlignmentForImage(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (productHelper.imagePitchAlignmentWARequired(hwInfo)) {
        return 64u;
    }
    return 4u;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen12);
}

template <>
const EngineInstancesContainer GfxCoreHelperHw<Family>::getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto defaultEngine = getChosenEngineType(hwInfo);

    EngineInstancesContainer engines;

    if (defaultEngine == aub_stream::EngineType::ENGINE_CCS && hwInfo.featureTable.flags.ftrCCSNode) {
        engines.push_back({aub_stream::ENGINE_CCS, EngineUsage::regular});
    }

    engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::regular});
    engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}); // low priority
    engines.push_back({defaultEngine, EngineUsage::internal});             // internal usage

    if (hwInfo.capabilityTable.blitterOperationsSupported) {
        if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
            engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::regular}); // Main copy engine
            if (!hwInfo.capabilityTable.isIntegratedDevice) {
                engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::internal}); // internal usage
            }
        }
    }

    return engines;
};

template <>
EngineGroupType GfxCoreHelperHw<Family>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    switch (engineType) {
    case aub_stream::ENGINE_RCS:
        return EngineGroupType::renderCompute;
    case aub_stream::ENGINE_CCS:
        return EngineGroupType::compute;
    case aub_stream::ENGINE_BCS:
        return EngineGroupType::copy;
    default:
        UNRECOVERABLE_IF(true);
    }
}

template <>
inline void MemorySynchronizationCommands<Family>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);

    if (debugManager.flags.FlushAllCaches.get()) {
        pipeControl.setHdcPipelineFlush(true);
    }
    if (debugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setHdcPipelineFlush(false);
    }
}

template <>
void MemorySynchronizationCommands<Family>::setCacheFlushExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
    args.constantCacheInvalidationEnable = false;
}

template <>
bool GfxCoreHelperHw<Family>::useOnlyGlobalTimestamps() const {
    return true;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        if (debugManager.flags.ForceL1Caching.get() != 1) {
            l1enabled = false;
        }

        if (l1enabled) {
            return gmmHelper.getL1EnabledMOCS() >> 1;
        } else {
            return gmmHelper.getL3EnabledMOCS() >> 1;
        }
    }

    return gmmHelper.getUncachedMOCS() >> 1;
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    return productHelper.pipeControlWARequired(hwInfo);
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierPriorToPipelineSelectWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return MemorySynchronizationCommands<Family>::isBarrierWaRequired(rootDeviceEnvironment);
}

template <>
void GfxCoreHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (productHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::cpuAccessDisallowed) {
        if (GraphicsAllocation::isCpuAccessRequired(properties.allocationType)) {
            allocationData.flags.useSystemMemory = true;
        }
    }
    if (productHelper.isStorageInfoAdjustmentRequired()) {
        if (properties.allocationType == AllocationType::buffer && !properties.flags.preferCompressed && !properties.flags.shareable) {
            allocationData.storageInfo.isLockable = true;
        }
    }
}

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO

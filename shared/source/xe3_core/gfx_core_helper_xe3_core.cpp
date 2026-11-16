/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

using Family = NEO::Xe3CoreFamily;

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_dg2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_pvc_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe3_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"
#include "shared/source/helpers/simd_helper.h"

namespace NEO {
template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Xe3);
}

template <>
aub_stream::MMIOList GfxCoreHelperHw<Family>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    aub_stream::MMIOList mmioList;

    if (compressedBuffersSupported(hwInfo) || compressedImagesSupported(hwInfo)) {
        uint32_t format = gmmHelper.getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

        if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
            format = static_cast<uint32_t>(debugManager.flags.ForceBufferCompressionFormat.get());
        }

        UNRECOVERABLE_IF(format > 0xF);

        mmioList.push_back({0x4148, format}); // [0:3] compression format
    }

    return mmioList;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForSingleAdditionalSynchronization(NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const bool globalFenceRequired = (fenceType == FenceType::acquire)
                                         ? productHelper.isAcquireGlobalFenceInDirectSubmissionRequired(hwInfo)
                                         : productHelper.isReleaseGlobalFenceInCommandStreamRequired(hwInfo);
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = globalFenceRequired ? AdditionalSynchronizationType::fence : AdditionalSynchronizationType::none;
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = static_cast<AdditionalSynchronizationType>(debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get());
    }

    if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::fence) {
        return sizeof(Family::MI_MEM_FENCE);
    } else if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::semaphore) {
        return EncodeSemaphore<Family>::getSizeMiSemaphoreWait();
    }
    return 0;
}

#include "shared/source/helpers/gfx_core_helper_from_xe2_hpg_to_xe3.inl"
#include "shared/source/helpers/gfx_core_helper_from_xe3_to_xe3p.inl"

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForAdditionalSynchronization(NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    return (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1 ? 2 : 1) * getSizeForSingleAdditionalSynchronization(fenceType, rootDeviceEnvironment);
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierWaFlags(void *barrierCmd) {
}

template <>
inline void MemorySynchronizationCommands<Family>::setPipeControlExtraProperties(Family::PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
    pipeControl.setDataportFlush(args.hdcPipelineFlush);
    pipeControl.setUnTypedDataPortCacheFlush(args.unTypedDataPortCacheFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);
    pipeControl.setWorkloadPartitionIdOffsetEnable(args.workloadPartitionOffset);
    pipeControl.setAmfsFlushEnable(args.amfsFlushEnable);

    if (debugManager.flags.FlushAllCaches.get()) {
        pipeControl.setDataportFlush(true);
        pipeControl.setUnTypedDataPortCacheFlush(true);
        pipeControl.setCompressionControlSurfaceCcsFlush(true);
    }
    if (debugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setDataportFlush(false);
        pipeControl.setUnTypedDataPortCacheFlush(false);
        pipeControl.setCompressionControlSurfaceCcsFlush(false);
    }
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierWa(void *&commandsBuffer, uint64_t gpuAddress, const RootDeviceEnvironment &rootDeviceEnvironment, NEO::PostSyncMode postSyncMode) {
}

template <>
void GfxCoreHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (hwInfo.featureTable.flags.ftrLocalMemory) {
        if (properties.allocationType == AllocationType::timestampPacketTagBuffer ||
            properties.allocationType == AllocationType::commandBuffer) {
            allocationData.flags.useSystemMemory = false;
        }

        if (properties.allocationType == AllocationType::semaphoreBuffer && !rootDeviceEnvironment.getProductHelper().isAcquireGlobalFenceInDirectSubmissionRequired(hwInfo)) {
            allocationData.flags.useSystemMemory = true;
        }
    }
}

template <>
size_t GfxCoreHelperHw<Family>::getTimestampPacketAllocatorAlignment() const {
    return MemoryConstants::cacheLineSize;
}

template <>
size_t GfxCoreHelperHw<Family>::getPreemptionAllocationAlignment() const {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = Family::STATE_CONTEXT_DATA_BASE_ADDRESS;
    return STATE_CONTEXT_DATA_BASE_ADDRESS::CONTEXTDATABASEADDRESS::CONTEXTDATABASEADDRESS_ALIGN_SIZE;
}

template <>
bool GfxCoreHelperHw<Family>::isMidThreadPreemptionAllocationOnDemandSupported() const {
    return true;
}

template <>
uint32_t GfxCoreHelperHw<Family>::adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    const uint32_t threadsPerThreadGroup = calculateNumThreadsPerThreadGroup(simd, defaultMaxGroupSize, grfCount, rootDeviceEnvironment);
    return (threadsPerThreadGroup * simd);
}

} // namespace NEO

namespace NEO {
#include "shared/source/helpers/gfx_core_helper_factory_init.inl"

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO

/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
void ProductHelperHw<IGFX_UNKNOWN>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    return 0;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    return 0;
}

template <>
void ProductHelperHw<IGFX_UNKNOWN>::setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) const {
}

template <>
void ProductHelperHw<IGFX_UNKNOWN>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getHostMemCapabilities(const HardwareInfo *hwInfo) const {
    return 0;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getDeviceMemCapabilities() const {
    return 0;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getSingleDeviceSharedMemCapabilities(bool) const {
    return 0;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getCrossDeviceSharedMemCapabilities() const {
    return 0;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::useGemCreateExtInAllocateMemoryByKMD() const {
    return false;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const {
    return 0;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::overrideGfxPartitionLayoutForWsl() const {
    return false;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 0;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 0;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 0;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isBlitterFullySupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (getSteppingFromHwRevId(hwInfo)) {
    default:
    case REVISION_A0:
    case REVISION_A1:
    case REVISION_A3:
        return AubMemDump::SteppingValues::A;
    case REVISION_B:
        return AubMemDump::SteppingValues::B;
    case REVISION_C:
        return AubMemDump::SteppingValues::C;
    case REVISION_D:
        return AubMemDump::SteppingValues::D;
    case REVISION_K:
        return AubMemDump::SteppingValues::K;
    }
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDirectSubmissionConstantCacheInvalidationNeeded(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
LocalMemoryAccessMode ProductHelperHw<IGFX_UNKNOWN>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::defaultMode;
}

template <>
LocalMemoryAccessMode ProductHelperHw<IGFX_UNKNOWN>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::defaultMode;
}

template <>
std::vector<int32_t> ProductHelperHw<IGFX_UNKNOWN>::getKernelSupportedThreadArbitrationPolicies() const {
    return {};
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isNewResidencyModelSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::deferMOCSToPatIndex(bool isWddmOnLinux) const {
    return false;
}

template <>
std::pair<bool, bool> ProductHelperHw<IGFX_UNKNOWN>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs, const ReleaseHelper *releaseHelper) const {
    return {false, false};
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::heapInLocalMem(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::canShareMemoryWithoutNTHandle() const {
    return true;
}

template <>
void ProductHelperHw<IGFX_UNKNOWN>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const {
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isCopyBufferRectSplitSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isAdditionalMediaSamplerProgrammingRequired() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isInitialFlagsProgrammingRequired() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::is3DPipelineSelectWARequired() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isStorageInfoAdjustmentRequired() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isBlitterForImagesSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDcFlushAllowed() const {
    return true;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.MaxSubSlicesSupported;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isReleaseGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return 8u;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isVmBindPatIndexProgrammingSupported() const {
    return false;
}

template <>
void ProductHelperHw<IGFX_UNKNOWN>::updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) const {
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isTimestampWaitSupportedForEvents() const {
    return false;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getInternalHeapsPreallocated() const {
    return 0u;
}

template <>
uint64_t ProductHelperHw<IGFX_UNKNOWN>::getHostMemCapabilitiesValue() const {
    return 0;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isEvictionIfNecessaryFlagSupported() const {
    return true;
}

template <>
const char *L1CachePolicyHelper<IGFX_UNKNOWN>::getCachingPolicyOptions(bool isDebuggerActive) {
    return nullptr;
}

template <>
uint32_t L1CachePolicyHelper<IGFX_UNKNOWN>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    return 0u;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isPrefetcherDisablingInDirectSubmissionRequired() const {
    return true;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getMaxNumSamplers() const {
    return 0u;
}

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getCommandBuffersPreallocatedPerCommandQueue() const {
    return 0u;
}

template <>
uint32_t L1CachePolicyHelper<IGFX_UNKNOWN>::getL1CachePolicy(bool isDebuggerActive) {
    return L1CachePolicyHelper<IGFX_UNKNOWN>::getDefaultL1CachePolicy(isDebuggerActive);
}

template <>
uint32_t L1CachePolicyHelper<IGFX_UNKNOWN>::getUncachedL1CachePolicy() {
    return 1u;
}

template <>
std::vector<uint32_t> ProductHelperHw<IGFX_UNKNOWN>::getSupportedNumGrfs(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->getSupportedNumGrfs();
    }
    return {};
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isBufferPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isHostUsmPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDeviceUsmPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isDeviceUsmAllocationReuseSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isHostUsmAllocationReuseSupported() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::useLocalPreferredForCacheableBuffers() const {
    return false;
}

template <>
std::optional<bool> ProductHelperHw<IGFX_UNKNOWN>::isCoherentAllocation(uint64_t patIndex) const {
    return std::nullopt;
}

struct UnknownProduct {
    struct FrontEndStateSupport {
        static constexpr bool scratchSize = false;
        static constexpr bool privateScratchSize = false;
        static constexpr bool computeDispatchAllWalker = false;

        static constexpr bool disableEuFusion = false;
        static constexpr bool disableOverdispatch = false;
        static constexpr bool singleSliceDispatchCcsMode = false;
    };

    struct StateComputeModeStateSupport {
        static constexpr bool threadArbitrationPolicy = false;
        static constexpr bool coherencyRequired = false;
        static constexpr bool largeGrfMode = false;
        static constexpr bool zPassAsyncComputeThreadLimit = false;
        static constexpr bool pixelAsyncComputeThreadLimit = false;
        static constexpr bool devicePreemptionMode = false;
    };

    struct StateBaseAddressStateSupport {
        static constexpr bool bindingTablePoolBaseAddress = false;
    };

    struct PipelineSelectStateSupport {
        static constexpr bool systolicMode = false;
    };

    struct PreemptionDebugSupport {
        static constexpr bool preemptionMode = false;
        static constexpr bool stateSip = false;
        static constexpr bool csrSurface = false;
    };

    static const uint32_t threadsPerEu = 8;
};

template <>
struct HwMapper<IGFX_UNKNOWN> {
    enum { gfxFamily = IGFX_UNKNOWN_CORE };

    static const char *abbreviation;
    using GfxProduct = UnknownProduct;
};

template <>
uint32_t ProductHelperHw<IGFX_UNKNOWN>::getCacheLineSize() const {
    return 0x40;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::is48bResourceNeededForRayTracing() const {
    return true;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isCompressionForbidden(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
void ProductHelperHw<IGFX_UNKNOWN>::setRenderCompressedFlags(HardwareInfo &hwInfo) const {}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isResourceUncachedForCS(AllocationType allocationType) const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isNonCoherentTimestampsModeEnabled() const {
    return false;
}

template <>
bool ProductHelperHw<IGFX_UNKNOWN>::isPidFdOrSocketForIpcSupported() const {
    return false;
}

template <>
void ProductHelperHw<IGFX_UNKNOWN>::overrideDirectSubmissionTimeouts(uint64_t &timeoutUs, uint64_t &maxTimeoutUs) const {
}

} // namespace NEO

#include "shared/source/os_interface/product_helper.inl"

template class NEO::ProductHelperHw<IGFX_UNKNOWN>;
template struct NEO::L1CachePolicyHelper<IGFX_UNKNOWN>;

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/unified_memory/usm_memory_support.h"

#include <bitset>

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    enableCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = 0u;
    *fp64 = 0u;
}

template <PRODUCT_FAMILY gfxProduct>
std::vector<int32_t> HwInfoConfigHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return PreambleHelper<GfxFamily>::getSupportedThreadArbitrationPolicies();
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::convertTimestampsFromOaToCsDomain(uint64_t &timestampData){};

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::enableBlitterOperationsSupport(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.blitterOperationsSupported = obtainBlitterPreference(*hwInfo);

    if (DebugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        hwInfo->capabilityTable.blitterOperationsSupported = !!DebugManager.flags.EnableBlitterOperationsSupport.get();
    }
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getDeviceMemCapabilities() {
    uint64_t capabilities = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::Device)) {
        capabilities |= UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() {
    uint64_t capabilities = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::SharedSingleDevice)) {
        capabilities |= UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getHostMemCapabilities(const HardwareInfo *hwInfo) {
    bool supported = getHostMemCapabilitiesSupported(hwInfo);

    if (DebugManager.flags.EnableHostUsmSupport.get() != -1) {
        supported = !!DebugManager.flags.EnableHostUsmSupport.get();
    }

    uint64_t capabilities = getHostMemCapabilitiesValue();

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::Host)) {
        capabilities |= UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
    }

    return (supported ? capabilities : 0);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) {
    bool supported = false;

    if (DebugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        supported = !!DebugManager.flags.EnableSharedSystemUsmSupport.get();
    }

    return (supported ? (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS) : 0);
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities capability) {
    auto supported = false;

    if (DebugManager.flags.EnableUsmConcurrentAccessSupport.get() > 0) {
        auto capabilityBitset = std::bitset<4>(DebugManager.flags.EnableUsmConcurrentAccessSupport.get());
        supported = capabilityBitset.test(static_cast<uint32_t>(capability));
    }

    return supported;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    uint32_t numThreadsPerEU = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    return maxNumEUsPerSubSlice * numThreadsPerEU;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::updateIddCommand(void *const commandPtr, uint32_t numGrf, int32_t threadArbitrationPolicy) {}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::overrideGfxPartitionLayoutForWsl() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
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

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::string HwInfoConfigHw<gfxProduct>::getDeviceMemoryName() const {
    return "DDR";
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getFrontEndPropertyDisableOverDispatchSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::allowCompression(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::allowStatelessCompression(const HardwareInfo &hwInfo) const {
    if (!NEO::ApiSpecificConfig::isStatelessCompressionSupported()) {
        return false;
    }
    if (DebugManager.flags.EnableStatelessCompression.get() != -1) {
        return static_cast<bool>(DebugManager.flags.EnableStatelessCompression.get());
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode HwInfoConfigHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::Default;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode HwInfoConfigHw<gfxProduct>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    switch (static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get())) {
    case LocalMemoryAccessMode::Default:
    case LocalMemoryAccessMode::CpuAccessAllowed:
    case LocalMemoryAccessMode::CpuAccessDisallowed:
        return static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get());
    }
    return getDefaultLocalMemoryAccessMode(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAssignEngineRoundRobinSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::pair<bool, bool> HwInfoConfigHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    return {false, false};
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAdditionalMediaSamplerProgrammingRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isInitialFlagsProgrammingRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::extraParametersInvalid(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBlitterForImagesSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isDcFlushAllowed() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.MaxSubSlicesSupported;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isFlushTaskAllowed() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::programAllStateComputeCommandFields() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return getPipelineSelectPropertySystolicModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const {
    return HwInfoConfigHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(hwInfo);
};

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return 8u;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const {
    return getFrontEndPropertyComputeDispatchAllWalkerSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isVmBindPatIndexProgrammingSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isGrfNumReportedWithScm() const {
    if (DebugManager.flags.ForceGrfNumProgrammingWithScm.get() != -1) {
        return DebugManager.flags.ForceGrfNumProgrammingWithScm.get();
    }
    return HwInfoConfigHw<gfxProduct>::getScmPropertyLargeGrfModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isThreadArbitrationPolicyReportedWithScm() const {
    if (DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.get() != -1) {
        return DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.get();
    }
    return HwInfoConfigHw<gfxProduct>::getScmPropertyThreadArbitrationPolicySupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() != -1) {
        return DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::allowMemoryPrefetch(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.EnableMemoryPrefetch.get() != -1) {
        return !!DebugManager.flags.EnableMemoryPrefetch.get();
    }
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBcsReportWaRequired(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.DoNotReportTile1BscWaActive.get() != -1) {
        return DebugManager.flags.DoNotReportTile1BscWaActive.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBlitSplitEnqueueWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const {
    return allocation.isAllocatedInLocalMemoryPool() &&
           (HwInfoConfig::get(hwInfo.platform.eProductFamily)->getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed ||
            !allocation.isAllocationLockable());
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isImplicitScalingSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const { return false; }

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getL1CachePolicy(bool isDebuggerActive) const {
    return L1CachePolicyHelper<gfxProduct>::getL1CachePolicy(isDebuggerActive);
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPrefetcherDisablingInDirectSubmissionRequired() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isStatefulAddressingModeSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPlatformQuerySupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isNonBlockingGpuSubmissionSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) {
    propertiesSupport.coherencyRequired = getScmPropertyCoherencyRequiredSupport();
    propertiesSupport.threadArbitrationPolicy = isThreadArbitrationPolicyReportedWithScm();
    propertiesSupport.largeGrfMode = isGrfNumReportedWithScm();
    propertiesSupport.zPassAsyncComputeThreadLimit = getScmPropertyZPassAsyncComputeThreadLimitSupport();
    propertiesSupport.pixelAsyncComputeThreadLimit = getScmPropertyPixelAsyncComputeThreadLimitSupport();
    propertiesSupport.devicePreemptionMode = getScmPropertyDevicePreemptionModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) {
    fillScmPropertiesSupportStructureBase(propertiesSupport);
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getScmPropertyThreadArbitrationPolicySupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::threadArbitrationPolicy;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getScmPropertyCoherencyRequiredSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::coherencyRequired;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getScmPropertyZPassAsyncComputeThreadLimitSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::zPassAsyncComputeThreadLimit;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getScmPropertyPixelAsyncComputeThreadLimitSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::pixelAsyncComputeThreadLimit;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getScmPropertyLargeGrfModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::largeGrfMode;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getScmPropertyDevicePreemptionModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::devicePreemptionMode;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getSbaPropertyGlobalAtomicsSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateBaseAddressStateSupport::globalAtomics;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getSbaPropertyStatelessMocsSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateBaseAddressStateSupport::statelessMocs;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getPreemptionDbgPropertyPreemptionModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PreemptionDebugSupport::preemptionMode;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getPreemptionDbgPropertyStateSipSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PreemptionDebugSupport::stateSip;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getPreemptionDbgPropertyCsrSurfaceSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PreemptionDebugSupport::csrSurface;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getFrontEndPropertyScratchSizeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::scratchSize;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getFrontEndPropertyPrivateScratchSizeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::privateScratchSize;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getFrontEndPropertyComputeDispatchAllWalkerSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::computeDispatchAllWalker;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getFrontEndPropertyDisableEuFusionSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::disableEuFusion;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getFrontEndPropertyDisableOverDispatchSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::disableOverdispatch;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getFrontEndPropertySingleSliceDispatchCcsModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::singleSliceDispatchCcsMode;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) {
    propertiesSupport.computeDispatchAllWalker = isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo);
    propertiesSupport.disableEuFusion = getFrontEndPropertyDisableEuFusionSupport();
    propertiesSupport.disableOverdispatch = isDisableOverdispatchAvailable(hwInfo);
    propertiesSupport.singleSliceDispatchCcsMode = getFrontEndPropertySingleSliceDispatchCcsModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getPipelineSelectPropertyModeSelectedSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PipelineSelectStateSupport::modeSelected;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PipelineSelectStateSupport::mediaSamplerDopClockGate;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getPipelineSelectPropertySystolicModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PipelineSelectStateSupport::systolicMode;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) {
    propertiesSupport.modeSelected = getPipelineSelectPropertyModeSelectedSupport();
    propertiesSupport.mediaSamplerDopClockGate = getPipelineSelectPropertyMediaSamplerDopClockGateSupport();
    propertiesSupport.systolicMode = isSystolicModeConfigurable(hwInfo);
}

} // namespace NEO

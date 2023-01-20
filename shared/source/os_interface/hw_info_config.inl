/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/unified_memory/usm_memory_support.h"

#include <bitset>

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) const {
    *fp16 = 0u;
    *fp32 = 0u;
    *fp64 = 0u;
}

template <PRODUCT_FAMILY gfxProduct>
std::vector<int32_t> ProductHelperHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() const {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return PreambleHelper<GfxFamily>::getSupportedThreadArbitrationPolicies();
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::enableBlitterOperationsSupport(HardwareInfo *hwInfo) const {
    hwInfo->capabilityTable.blitterOperationsSupported = obtainBlitterPreference(*hwInfo);

    if (DebugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        hwInfo->capabilityTable.blitterOperationsSupported = !!DebugManager.flags.EnableBlitterOperationsSupport.get();
    }
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::disableRcsExposure(HardwareInfo *hwInfo) const {
    hwInfo->featureTable.flags.ftrRcsNode = false;
    if ((DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) || (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS))) {
        hwInfo->featureTable.flags.ftrRcsNode = true;
    }
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemCapabilities() const {
    uint64_t capabilities = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::Device)) {
        capabilities |= UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() const {
    uint64_t capabilities = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::SharedSingleDevice)) {
        capabilities |= UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilities(const HardwareInfo *hwInfo) const {
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
uint64_t ProductHelperHw<gfxProduct>::getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const {
    bool supported = false;

    if (DebugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        supported = !!DebugManager.flags.EnableSharedSystemUsmSupport.get();
    }

    return (supported ? (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS) : 0);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities capability) const {
    auto supported = false;

    if (DebugManager.flags.EnableUsmConcurrentAccessSupport.get() > 0) {
        auto capabilityBitset = std::bitset<4>(DebugManager.flags.EnableUsmConcurrentAccessSupport.get());
        supported = capabilityBitset.test(static_cast<uint32_t>(capability));
    }

    return supported;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    uint32_t numThreadsPerEU = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    return maxNumEUsPerSubSlice * numThreadsPerEU;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) const {}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) const {}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::updateIddCommand(void *const commandPtr, uint32_t numGrf, int32_t threadArbitrationPolicy) const {}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::overrideGfxPartitionLayoutForWsl() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
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
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return std::nullopt;
};

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::string ProductHelperHw<gfxProduct>::getDeviceMemoryName() const {
    return "DDR";
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getFrontEndPropertyDisableOverDispatchSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::allowCompression(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode ProductHelperHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::Default;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode ProductHelperHw<gfxProduct>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    switch (static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get())) {
    case LocalMemoryAccessMode::Default:
    case LocalMemoryAccessMode::CpuAccessAllowed:
    case LocalMemoryAccessMode::CpuAccessDisallowed:
        return static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get());
    }
    return getDefaultLocalMemoryAccessMode(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAssignEngineRoundRobinSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::pair<bool, bool> ProductHelperHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    return {false, false};
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAdditionalMediaSamplerProgrammingRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isInitialFlagsProgrammingRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::extraParametersInvalid(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitterForImagesSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDcFlushAllowed() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.MaxSubSlicesSupported;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isFlushTaskAllowed() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::programAllStateComputeCommandFields() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return getPipelineSelectPropertySystolicModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const {
    return ProductHelperHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(hwInfo);
};

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return 8u;
}

template <PRODUCT_FAMILY gfxProduct>
size_t ProductHelperHw<gfxProduct>::getSvmCpuAlignment() const {
    return MemoryConstants::pageSize2Mb;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const {
    return getFrontEndPropertyComputeDispatchAllWalkerSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isVmBindPatIndexProgrammingSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isGrfNumReportedWithScm() const {
    if (DebugManager.flags.ForceGrfNumProgrammingWithScm.get() != -1) {
        return DebugManager.flags.ForceGrfNumProgrammingWithScm.get();
    }
    return ProductHelperHw<gfxProduct>::getScmPropertyLargeGrfModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isThreadArbitrationPolicyReportedWithScm() const {
    if (DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.get() != -1) {
        return DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.get();
    }
    return ProductHelperHw<gfxProduct>::getScmPropertyThreadArbitrationPolicySupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() != -1) {
        return DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::allowMemoryPrefetch(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.EnableMemoryPrefetch.get() != -1) {
        return !!DebugManager.flags.EnableMemoryPrefetch.get();
    }
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBcsReportWaRequired(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.DoNotReportTile1BscWaActive.get() != -1) {
        return DebugManager.flags.DoNotReportTile1BscWaActive.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitSplitEnqueueWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const {
    return allocation.isAllocatedInLocalMemoryPool() &&
           (ProductHelper::get(hwInfo.platform.eProductFamily)->getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed ||
            !allocation.isAllocationLockable());
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isImplicitScalingSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const { return false; }

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getL1CachePolicy(bool isDebuggerActive) const {
    return L1CachePolicyHelper<gfxProduct>::getL1CachePolicy(isDebuggerActive);
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isPrefetcherDisablingInDirectSubmissionRequired() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isStatefulAddressingModeSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isPlatformQuerySupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isNonBlockingGpuSubmissionSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ) const {
    constexpr bool enabled = false;
    if (DebugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return DebugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isMidThreadPreemptionDisallowedForRayTracingKernels() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) const {
    propertiesSupport.coherencyRequired = getScmPropertyCoherencyRequiredSupport();
    propertiesSupport.threadArbitrationPolicy = isThreadArbitrationPolicyReportedWithScm();
    propertiesSupport.largeGrfMode = isGrfNumReportedWithScm();
    propertiesSupport.zPassAsyncComputeThreadLimit = getScmPropertyZPassAsyncComputeThreadLimitSupport();
    propertiesSupport.pixelAsyncComputeThreadLimit = getScmPropertyPixelAsyncComputeThreadLimitSupport();
    propertiesSupport.devicePreemptionMode = getScmPropertyDevicePreemptionModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) const {
    fillScmPropertiesSupportStructureBase(propertiesSupport);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getScmPropertyThreadArbitrationPolicySupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::threadArbitrationPolicy;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getScmPropertyCoherencyRequiredSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::coherencyRequired;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getScmPropertyZPassAsyncComputeThreadLimitSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::zPassAsyncComputeThreadLimit;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getScmPropertyPixelAsyncComputeThreadLimitSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::pixelAsyncComputeThreadLimit;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getScmPropertyLargeGrfModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::largeGrfMode;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getScmPropertyDevicePreemptionModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateComputeModeStateSupport::devicePreemptionMode;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getStateBaseAddressPropertyGlobalAtomicsSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateBaseAddressStateSupport::globalAtomics;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getStateBaseAddressPropertyStatelessMocsSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateBaseAddressStateSupport::statelessMocs;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateBaseAddressStateSupport::bindingTablePoolBaseAddress;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::fillStateBaseAddressPropertiesSupportStructure(StateBaseAddressPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const {
    propertiesSupport.globalAtomics = getStateBaseAddressPropertyGlobalAtomicsSupport();
    propertiesSupport.statelessMocs = getStateBaseAddressPropertyStatelessMocsSupport();
    propertiesSupport.bindingTablePoolBaseAddress = getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getPreemptionDbgPropertyPreemptionModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PreemptionDebugSupport::preemptionMode;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getPreemptionDbgPropertyStateSipSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PreemptionDebugSupport::stateSip;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getPreemptionDbgPropertyCsrSurfaceSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PreemptionDebugSupport::csrSurface;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getFrontEndPropertyScratchSizeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::scratchSize;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getFrontEndPropertyPrivateScratchSizeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::privateScratchSize;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getFrontEndPropertyComputeDispatchAllWalkerSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::computeDispatchAllWalker;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getFrontEndPropertyDisableEuFusionSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::disableEuFusion;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getFrontEndPropertyDisableOverDispatchSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::disableOverdispatch;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getFrontEndPropertySingleSliceDispatchCcsModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::FrontEndStateSupport::singleSliceDispatchCcsMode;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const {
    propertiesSupport.computeDispatchAllWalker = isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo);
    propertiesSupport.disableEuFusion = getFrontEndPropertyDisableEuFusionSupport();
    propertiesSupport.disableOverdispatch = isDisableOverdispatchAvailable(hwInfo);
    propertiesSupport.singleSliceDispatchCcsMode = getFrontEndPropertySingleSliceDispatchCcsModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getPipelineSelectPropertyModeSelectedSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PipelineSelectStateSupport::modeSelected;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PipelineSelectStateSupport::mediaSamplerDopClockGate;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getPipelineSelectPropertySystolicModeSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::PipelineSelectStateSupport::systolicMode;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const {
    propertiesSupport.modeSelected = getPipelineSelectPropertyModeSelectedSupport();
    propertiesSupport.mediaSamplerDopClockGate = getPipelineSelectPropertyMediaSamplerDopClockGateSupport();
    propertiesSupport.systolicMode = isSystolicModeConfigurable(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getDefaultRevisionId() const {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::overridePatIndex(AllocationType allocationType, uint64_t patIndex) const {
    return patIndex;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isMultiContextResourceDeferDeletionSupported() const {
    return false;
}

} // namespace NEO

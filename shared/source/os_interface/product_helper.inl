/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/definitions/indirect_detection_versions.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/utilities/logger.h"

#include "aubstream/engine_node.h"
#include "ocl_igc_shared/indirect_access_detection/version.h"

#include <bitset>
#include <limits>

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
std::vector<int32_t> ProductHelperHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() const {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return PreambleHelper<GfxFamily>::getSupportedThreadArbitrationPolicies();
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
    using SAMPLER_STATE = typename HwMapper<gfxProduct>::GfxFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (debugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    bool tlbFlushRequired = true;
    if (debugManager.flags.ForceTlbFlush.get() != -1) {
        tlbFlushRequired = !!debugManager.flags.ForceTlbFlush.get();
    }
    return tlbFlushRequired;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDetectIndirectAccessInKernelSupported(const KernelDescriptor &kernelDescriptor, const bool isPrecompiled, const uint32_t precompiledKernelIndirectDetectionVersion) const {
    const bool isCMKernelHeuristic = kernelDescriptor.kernelAttributes.simdSize == 1;
    const bool isZebin = kernelDescriptor.kernelAttributes.binaryFormat == DeviceBinaryFormat::zebin;
    const auto currentIndirectDetectionVersion = isPrecompiled ? precompiledKernelIndirectDetectionVersion : INDIRECT_ACCESS_DETECTION_VERSION;
    bool indirectDetectionValid = false;
    NEO::fileLoggerInstance().log(!!debugManager.flags.LogIndirectDetectionKernelDetails.get(),
                                  "kernelName,", kernelDescriptor.kernelMetadata.kernelName,
                                  "isPrecompiled,", isPrecompiled,
                                  "isZebin,", isZebin,
                                  "isCMKernelHeuristic,", isCMKernelHeuristic,
                                  "driver indirect detection version,", INDIRECT_ACCESS_DETECTION_VERSION,
                                  "precompiled kernel indirect detection version,", precompiledKernelIndirectDetectionVersion,
                                  "hasNonKernelArgLoad,", kernelDescriptor.kernelAttributes.hasNonKernelArgLoad,
                                  "hasNonKernelArgStore,", kernelDescriptor.kernelAttributes.hasNonKernelArgStore,
                                  "hasNonKernelArgAtomic,", kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic,
                                  "hasIndirectStatelessAccess,", kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess,
                                  "hasIndirectAccessInImplicitArg,", kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg,
                                  "useStackCalls,", kernelDescriptor.kernelAttributes.flags.useStackCalls,
                                  "isAnyArgumentPtrByValue,", KernelHelper::isAnyArgumentPtrByValue(kernelDescriptor),
                                  "\n");
    if (debugManager.flags.DisableIndirectDetectionForKernelNames.get() != "unk") {
        if (kernelDescriptor.kernelMetadata.kernelName.find(debugManager.flags.DisableIndirectDetectionForKernelNames.get()) != std::string::npos ||
            debugManager.flags.DisableIndirectDetectionForKernelNames.get().find(kernelDescriptor.kernelMetadata.kernelName) != std::string::npos) {
            return false;
        }
    }
    if (isCMKernelHeuristic && debugManager.flags.ForceIndirectDetectionForCMKernels.get() != -1) {
        return debugManager.flags.ForceIndirectDetectionForCMKernels.get() == 1;
    }
    if (isCMKernelHeuristic) {
        if (IndirectDetectionVersions::disabled == getRequiredDetectIndirectVersionVC()) {
            return false;
        }
        indirectDetectionValid = currentIndirectDetectionVersion >= getRequiredDetectIndirectVersionVC();
    } else {
        if (IndirectDetectionVersions::disabled == getRequiredDetectIndirectVersion()) {
            return false;
        }
        indirectDetectionValid = currentIndirectDetectionVersion >= getRequiredDetectIndirectVersion();
    }
    return isZebin && indirectDetectionValid;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getRequiredDetectIndirectVersion() const {
    return IndirectDetectionVersions::disabled;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getRequiredDetectIndirectVersionVC() const {
    return IndirectDetectionVersions::requiredDetectIndirectVersionVectorCompiler;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::enableBlitterOperationsSupport(HardwareInfo *hwInfo) const {
    hwInfo->capabilityTable.blitterOperationsSupported = obtainBlitterPreference(*hwInfo);

    if (debugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        hwInfo->capabilityTable.blitterOperationsSupported = !!debugManager.flags.EnableBlitterOperationsSupport.get();
    }
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemCapabilities() const {
    uint64_t capabilities = UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::device)) {
        capabilities |= UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() const {
    uint64_t capabilities = UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess;

    if (isKmdMigrationSupported() || getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::sharedSingleDevice)) {
        capabilities |= UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;
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

    if (debugManager.flags.EnableHostUsmSupport.get() != -1) {
        supported = !!debugManager.flags.EnableHostUsmSupport.get();
    }

    uint64_t capabilities = getHostMemCapabilitiesValue();

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::host)) {
        capabilities |= UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;
    }

    return (supported ? capabilities : 0);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const {
    bool supported = false;

    if (debugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        supported = !!debugManager.flags.EnableSharedSystemUsmSupport.get();
    }

    return (supported ? (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess) : 0);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities capability) const {
    auto supported = false;

    if (debugManager.flags.EnableUsmConcurrentAccessSupport.get() > 0) {
        auto capabilityBitset = std::bitset<4>(debugManager.flags.EnableUsmConcurrentAccessSupport.get());
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
bool ProductHelperHw<gfxProduct>::overrideAllocationCacheable(const AllocationData &allocationData) const {
    return false;
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
bool ProductHelperHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getFrontEndPropertyDisableOverDispatchSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::allowCompression(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode ProductHelperHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::defaultMode;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode ProductHelperHw<gfxProduct>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    switch (static_cast<LocalMemoryAccessMode>(debugManager.flags.ForceLocalMemoryAccessMode.get())) {
    case LocalMemoryAccessMode::defaultMode:
    case LocalMemoryAccessMode::cpuAccessAllowed:
    case LocalMemoryAccessMode::cpuAccessDisallowed:
        return static_cast<LocalMemoryAccessMode>(debugManager.flags.ForceLocalMemoryAccessMode.get());
    }
    return getDefaultLocalMemoryAccessMode(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAssignEngineRoundRobinSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::pair<bool, bool> ProductHelperHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs, const ReleaseHelper *releaseHelper) const {
    auto isBasicWARequired = false;
    if (releaseHelper) {
        isBasicWARequired = releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired();
    }
    auto isExtendedWARequired = false;
    if (debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }
    return {isBasicWARequired, isExtendedWARequired};
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
bool ProductHelperHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isDirectSubmissionSupported();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionConstantCacheInvalidationNeeded(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::restartDirectSubmissionForHostptrFree() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAdjustDirectSubmissionTimeoutOnThrottleAndAcLineStatusEnabled() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
TimeoutParams ProductHelperHw<gfxProduct>::getDirectSubmissionControllerTimeoutParams(bool acLineConnected, QueueThrottle queueThrottle) const {
    TimeoutParams params{};
    params.maxTimeout = std::chrono::microseconds{DirectSubmissionController::defaultTimeout};
    params.timeout = std::chrono::microseconds{DirectSubmissionController::defaultTimeout};
    params.timeoutDivisor = 1;
    params.directSubmissionEnabled = true;
    return params;
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
bool ProductHelperHw<gfxProduct>::isPageFaultSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::blitEnqueuePreferred(bool isWriteToImageFromBuffer) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isKmdMigrationSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDcFlushAllowed() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    bool dcFlushAllowed = GfxProduct::isDcFlushAllowed && !this->mitigateDcFlush();

    if (debugManager.flags.AllowDcFlush.get() != -1) {
        dcFlushAllowed = debugManager.flags.AllowDcFlush.get();
    }

    return dcFlushAllowed;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::mitigateDcFlush() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDcFlushMitigated() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    bool dcFlushAllowed = GfxProduct::isDcFlushAllowed;
    return this->isDcFlushAllowed() != dcFlushAllowed;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::overrideUsageForDcFlushMitigation(AllocationType allocationType) const {
    return this->isDcFlushMitigated() && (this->overridePatToUCAndTwoWayCohForDcFlushMitigation(allocationType) || overridePatToUCAndOneWayCohForDcFlushMitigation(allocationType));
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::overridePatToUCAndTwoWayCohForDcFlushMitigation(AllocationType allocationType) const {
    return this->isDcFlushMitigated() &&
           (this->overrideCacheableForDcFlushMitigation(allocationType) ||
            allocationType == AllocationType::timestampPacketTagBuffer ||
            allocationType == AllocationType::tagBuffer ||
            allocationType == AllocationType::gpuTimestampDeviceBuffer);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::overridePatToUCAndOneWayCohForDcFlushMitigation(AllocationType allocationType) const {
    return this->isDcFlushMitigated() &&
           (allocationType == AllocationType::internalHeap ||
            allocationType == AllocationType::linearStream);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::overrideCacheableForDcFlushMitigation(AllocationType allocationType) const {
    return this->isDcFlushMitigated() &&
           (allocationType == AllocationType::externalHostPtr ||
            allocationType == AllocationType::bufferHostMemory ||
            allocationType == AllocationType::mapAllocation ||
            allocationType == AllocationType::svmCpu ||
            allocationType == AllocationType::svmZeroCopy ||
            allocationType == AllocationType::internalHostMemory ||
            allocationType == AllocationType::printfSurface);
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.MaxSubSlicesSupported;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isFlushTaskAllowed() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return getPipelineSelectPropertySystolicModeSupport();
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
uint32_t ProductHelperHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return 8u;
}

template <PRODUCT_FAMILY gfxProduct>
size_t ProductHelperHw<gfxProduct>::getSvmCpuAlignment() const {
    return MemoryConstants::pageSize2M;
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
    if (debugManager.flags.ForceGrfNumProgrammingWithScm.get() != -1) {
        return debugManager.flags.ForceGrfNumProgrammingWithScm.get();
    }
    return ProductHelperHw<gfxProduct>::getScmPropertyLargeGrfModeSupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isThreadArbitrationPolicyReportedWithScm() const {
    if (debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.get() != -1) {
        return debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.get();
    }
    return ProductHelperHw<gfxProduct>::getScmPropertyThreadArbitrationPolicySupport();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() != -1) {
        return debugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::allowMemoryPrefetch(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.EnableMemoryPrefetch.get() != -1) {
        return !!debugManager.flags.EnableMemoryPrefetch.get();
    }
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBcsReportWaRequired(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.DoNotReportTile1BscWaActive.get() != -1) {
        return debugManager.flags.DoNotReportTile1BscWaActive.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitSplitEnqueueWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitCopyRequiredForLocalMemory(const RootDeviceEnvironment &rootDeviceEnvironment, const GraphicsAllocation &allocation) const {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return allocation.isAllocatedInLocalMemoryPool() &&
           (productHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::cpuAccessDisallowed ||
            !allocation.isAllocationLockable());
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isInitDeviceWithFirstSubmissionRequired(const HardwareInfo &hwInfo) const {
    return false;
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
bool ProductHelperHw<gfxProduct>::isUnlockingLockedPtrNecessary(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAdjustWalkOrderAvailable(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isAdjustWalkOrderAvailable();
    }
    return false;
}

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
uint32_t ProductHelperHw<gfxProduct>::getNumberOfPartsInTileForConcurrentKernel(uint32_t ccsCount) const {
    return 1u;
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
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const {
    constexpr bool enabled = false;
    if (debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return debugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
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
void ProductHelperHw<gfxProduct>::fillScmPropertiesSupportStructureExtra(StateComputeModePropertiesSupport &propertiesSupport, const RootDeviceEnvironment &rootDeviceEnvironment) const {}

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
bool ProductHelperHw<gfxProduct>::getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::StateBaseAddressStateSupport::bindingTablePoolBaseAddress;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::fillStateBaseAddressPropertiesSupportStructure(StateBaseAddressPropertiesSupport &propertiesSupport) const {
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
    propertiesSupport.mediaSamplerDopClockGate = getPipelineSelectPropertyMediaSamplerDopClockGateSupport();
    propertiesSupport.systolicMode = isSystolicModeConfigurable(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isFusedEuDisabledForDpas(bool kernelHasDpasInstructions, const uint32_t *lws, const uint32_t *groupCount, const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCalculationForDisablingEuFusionWithDpasNeeded(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::is48bResourceNeededForRayTracing() const {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return EncodeEnableRayTracing<GfxFamily>::is48bResourceNeededForRayTracing();
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isLinearStoragePreferred(bool isImage1d, bool forceLinearStorage) const {
    if (debugManager.flags.ForceLinearImages.get() || forceLinearStorage || isImage1d) {
        return true;
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTranslationExceptionSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getMaxNumSamplers() const {
    return 16u;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::disableL3CacheForDebug(const HardwareInfo &) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isSkippingStatefulInformationRequired(const KernelDescriptor &kernelDescriptor) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isResolvingSubDeviceIDNeeded(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isResolvingSubDeviceIDNeeded();
    }
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::overridePatIndex(bool isUncachedType, uint64_t patIndex, AllocationType allocationType) const {
    return patIndex;
}

template <PRODUCT_FAMILY gfxProduct>
std::vector<uint32_t> ProductHelperHw<gfxProduct>::getSupportedNumGrfs(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->getSupportedNumGrfs();
    }
    return {128u};
}

template <PRODUCT_FAMILY gfxProduct>
aub_stream::EngineType ProductHelperHw<gfxProduct>::getDefaultCopyEngine() const {
    return aub_stream::EngineType::ENGINE_BCS;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::adjustEngineGroupType(EngineGroupType &engineGroupType) const {}

template <PRODUCT_FAMILY gfxProduct>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    return {};
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCachingOnCpuAvailable() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isNewCoherencyModelSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::supportReadOnlyAllocations() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::localDispatchSizeQuerySupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDeviceToHostCopySignalingFenceRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
size_t ProductHelperHw<gfxProduct>::getMaxFillPaternSizeForCopyEngine() const {
    return 4 * sizeof(uint32_t);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isAvailableExtendedScratch() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isStagingBuffersEnabled() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getCacheLineSize() const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    return GfxProduct::cacheLineSize;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::supports2DBlockLoad() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::supports2DBlockStore() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getNumCacheRegions() const {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const {
    UNRECOVERABLE_IF(true);
    return -1;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isEvictionIfNecessaryFlagSupported() const {
    return true;
}

} // namespace NEO

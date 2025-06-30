/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
class ProductHelperHw : public ProductHelper {
  public:
    static std::unique_ptr<ProductHelper> create() {
        auto productHelper = std::unique_ptr<ProductHelper>(new ProductHelperHw());

        return productHelper;
    }

    int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const override;
    void adjustPlatformForProductFamily(HardwareInfo *hwInfo) override;
    void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const override;
    uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) const override;
    uint64_t getDeviceMemCapabilities() const override;
    uint64_t getSingleDeviceSharedMemCapabilities(bool isKmdMigrationAvailable) const override;
    uint64_t getCrossDeviceSharedMemCapabilities() const override;
    uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override;
    std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() const override;
    uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    uint64_t getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    bool isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const override;
    bool isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const override;
    uint32_t getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const override;
    uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const override;
    uint32_t getPreferredWorkgroupCountPerSubslice() const override;
    void setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) const override;
    void updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) const override;
    bool obtainBlitterPreference(const HardwareInfo &hwInfo) const override;
    bool isBlitterFullySupported(const HardwareInfo &hwInfo) const override;
    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override;
    bool overrideGfxPartitionLayoutForWsl() const override;
    uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const override;
    uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    std::optional<aub_stream::ProductFamily> getAubStreamProductFamily() const override;
    bool isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const override;
    bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const override;
    LocalMemoryAccessMode getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const override;
    bool isNewResidencyModelSupported() const override;
    bool isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const override;
    bool isDirectSubmissionConstantCacheInvalidationNeeded(const HardwareInfo &hwInfo) const override;
    bool restartDirectSubmissionForHostptrFree() const override;
    std::pair<bool, bool> isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs, const ReleaseHelper *releaseHelper) const override;
    bool heapInLocalMem(const HardwareInfo &hwInfo) const override;
    void setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const override;
    bool isAdditionalMediaSamplerProgrammingRequired() const override;
    bool isInitialFlagsProgrammingRequired() const override;
    bool isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const override;
    bool pipeControlWARequired(const HardwareInfo &hwInfo) const override;
    bool imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const override;
    bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const override;
    bool is3DPipelineSelectWARequired() const override;
    bool isStorageInfoAdjustmentRequired() const override;
    bool isBlitterForImagesSupported() const override;
    bool isPageFaultSupported() const override;
    bool blitEnqueuePreferred(bool isWriteToImageFromBuffer) const override;
    bool isKmdMigrationSupported() const override;
    bool isDisableScratchPagesSupported() const override;
    bool isDisableScratchPagesRequiredForDebugger() const override;
    bool areSecondaryContextsSupported() const override;
    bool isPrimaryContextsAggregationSupported() const override;
    bool isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const override;
    bool isDcFlushAllowed() const override;
    uint32_t computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const override;
    bool getUuid(NEO::DriverModel *driverModel, uint32_t subDeviceCount, uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override;
    bool isSystolicModeConfigurable(const HardwareInfo &hwInfo) const override;
    bool isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const override;
    bool isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const override;
    bool isReleaseGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const override;
    bool isGlobalFenceInPostSyncRequired(const HardwareInfo &hwInfo) const override;
    bool isAcquireGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const override;
    bool isTimestampWaitSupportedForQueues(bool heaplessEnabled) const override;
    uint32_t getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const override;
    void adjustScratchSize(size_t &requiredScratchSize) const override;
    size_t getSvmCpuAlignment() const override;
    bool isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const override;
    bool adjustDispatchAllRequired(const HardwareInfo &hwInfo) const override;
    bool isVmBindPatIndexProgrammingSupported() const override;
    bool isIpSamplingSupported(const HardwareInfo &hwInfo) const override;
    bool isGrfNumReportedWithScm() const override;
    bool isThreadArbitrationPolicyReportedWithScm() const override;
    bool isCopyBufferRectSplitSupported() const override;
    bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override;
    bool isTimestampWaitSupportedForEvents() const override;
    bool isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const override;
    BcsSplitSettings getBcsSplitSettings() const override;
    bool isInitDeviceWithFirstSubmissionRequired(const HardwareInfo &hwInfo) const override;
    bool allowMemoryPrefetch(const HardwareInfo &hwInfo) const override;
    bool isBcsReportWaRequired(const HardwareInfo &hwInfo) const override;
    bool isBlitCopyRequiredForLocalMemory(const RootDeviceEnvironment &rootDeviceEnvironment, const GraphicsAllocation &allocation) const override;
    bool isImplicitScalingSupported(const HardwareInfo &hwInfo) const override;
    bool isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const override;
    bool isUnlockingLockedPtrNecessary(const HardwareInfo &hwInfo) const override;
    bool isAdjustWalkOrderAvailable(const ReleaseHelper *releaseHelper) const override;
    uint32_t getL1CachePolicy(bool isDebuggerActive) const override;
    bool isEvictionIfNecessaryFlagSupported() const override;
    void adjustNumberOfCcs(HardwareInfo &hwInfo) const override;
    bool isPrefetcherDisablingInDirectSubmissionRequired() const override;
    bool isStatefulAddressingModeSupported() const override;
    uint32_t getNumberOfPartsInTileForConcurrentKernel(uint32_t ccsCount) const override;
    bool isPlatformQuerySupported() const override;
    bool isNonBlockingGpuSubmissionSupported() const override;
    bool isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const override;
    bool isBufferPoolAllocatorSupported() const override;
    bool isHostUsmPoolAllocatorSupported() const override;
    bool isDeviceUsmPoolAllocatorSupported() const override;
    bool isDeviceUsmAllocationReuseSupported() const override;
    bool isHostUsmAllocationReuseSupported() const override;
    bool useLocalPreferredForCacheableBuffers() const override;
    bool useGemCreateExtInAllocateMemoryByKMD() const override;
    bool isTlbFlushRequired() const override;
    bool isDetectIndirectAccessInKernelSupported(const KernelDescriptor &kernelDescriptor, const bool isPrecompiled, const uint32_t precompiledKernelIndirectDetectionVersion) const override;
    uint32_t getRequiredDetectIndirectVersion() const override;
    uint32_t getRequiredDetectIndirectVersionVC() const override;
    bool isLinearStoragePreferred(bool isImage1d, bool forceLinearStorage) const override;
    bool isTranslationExceptionSupported() const override;
    uint32_t getMaxNumSamplers() const override;
    uint32_t getCommandBuffersPreallocatedPerCommandQueue() const override;
    uint32_t getInternalHeapsPreallocated() const override;
    bool overrideAllocationCpuCacheable(const AllocationData &allocationData) const override;
    bool is2MBLocalMemAlignmentEnabled() const override;
    bool isPackedCopyFormatSupported() const override;

    bool getFrontEndPropertyScratchSizeSupport() const override;
    bool getFrontEndPropertyPrivateScratchSizeSupport() const override;
    bool getFrontEndPropertyComputeDispatchAllWalkerSupport() const override;
    bool getFrontEndPropertyDisableEuFusionSupport() const override;
    bool getFrontEndPropertyDisableOverDispatchSupport() const override;
    bool getFrontEndPropertySingleSliceDispatchCcsModeSupport() const override;

    bool getScmPropertyThreadArbitrationPolicySupport() const override;
    bool getScmPropertyCoherencyRequiredSupport() const override;
    bool getScmPropertyZPassAsyncComputeThreadLimitSupport() const override;
    bool getScmPropertyPixelAsyncComputeThreadLimitSupport() const override;
    bool getScmPropertyLargeGrfModeSupport() const override;
    bool getScmPropertyDevicePreemptionModeSupport() const override;

    bool getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport() const override;

    bool getPreemptionDbgPropertyPreemptionModeSupport() const override;
    bool getPreemptionDbgPropertyStateSipSupport() const override;
    bool getPreemptionDbgPropertyCsrSurfaceSupport() const override;

    bool getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const override;
    bool getPipelineSelectPropertySystolicModeSupport() const override;

    void fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) const override;
    void fillScmPropertiesSupportStructureExtra(StateComputeModePropertiesSupport &propertiesSupport, const RootDeviceEnvironment &rootDeviceEnvironment) const override;
    void fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const override;
    void fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const override;
    void fillStateBaseAddressPropertiesSupportStructure(StateBaseAddressPropertiesSupport &propertiesSupport) const override;
    void parseCcsMode(std::string ccsModeString, std::unordered_map<uint32_t, uint32_t> &rootDeviceNumCcsMap, uint32_t rootDeviceIndex, RootDeviceEnvironment *rootDeviceEnvironment) const override;

    bool isFusedEuDisabledForDpas(bool kernelHasDpasInstructions, const uint32_t *lws, const uint32_t *groupCount, const HardwareInfo &hwInfo) const override;
    bool isCalculationForDisablingEuFusionWithDpasNeeded(const HardwareInfo &hwInfo) const override;
    bool is48bResourceNeededForRayTracing() const override;
    bool disableL3CacheForDebug(const HardwareInfo &hwInfo) const override;
    bool isSkippingStatefulInformationRequired(const KernelDescriptor &kernelDescriptor) const override;
    bool isResolvingSubDeviceIDNeeded(const ReleaseHelper *releaseHelper) const override;
    uint64_t overridePatIndex(bool isUncachedType, uint64_t patIndex, AllocationType allocationType) const override;
    std::vector<uint32_t> getSupportedNumGrfs(const ReleaseHelper *releaseHelper) const override;
    aub_stream::EngineType getDefaultCopyEngine() const override;
    void adjustEngineGroupType(EngineGroupType &engineGroupType) const override;
    std::optional<GfxMemoryAllocationMethod> getPreferredAllocationMethod(AllocationType allocationType) const override;
    bool isCachingOnCpuAvailable() const override;
    bool isNewCoherencyModelSupported() const override;
    bool isResourceUncachedForCS(AllocationType allocationType) const override;
    bool deferMOCSToPatIndex(bool isWddmOnLinux) const override;
    bool supportReadOnlyAllocations() const override;
    const std::vector<uint32_t> getSupportedLocalDispatchSizes(const HardwareInfo &hwInfo) const override;
    uint32_t getMaxLocalRegionSize(const HardwareInfo &hwInfo) const override;
    uint32_t getMaxLocalSubRegionSize(const HardwareInfo &hwInfo) const override;
    bool localDispatchSizeQuerySupported() const override;
    bool isDeviceToHostCopySignalingFenceRequired() const override;
    size_t getMaxFillPaternSizeForCopyEngine() const override;
    bool isAvailableExtendedScratch() const override;
    std::optional<bool> isCoherentAllocation(uint64_t patIndex) const override;
    bool isStagingBuffersEnabled() const override;
    uint32_t getCacheLineSize() const override;
    bool supports2DBlockStore() const override;
    bool supports2DBlockLoad() const override;
    uint32_t getNumCacheRegions() const override;
    uint32_t adjustMaxThreadsPerThreadGroup(uint32_t maxThreadsPerThreadGroup, uint32_t simt, uint32_t grfCount, bool isHeaplessModeEnabled) const override;
    uint64_t getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const override;
    uint32_t getGmmResourceUsageOverride(uint32_t usageType) const override;
    bool isSharingWith3dOrMediaAllowed() const override;
    bool isL3FlushAfterPostSyncRequired(bool heaplessEnabled) const override;
    void overrideDirectSubmissionTimeouts(uint64_t &timeoutUs, uint64_t &maxTimeoutUs) const override;
    bool isMisalignedUserPtr2WayCoherent() const override;
    bool isSvmHeapReservationSupported() const override;
    void setRenderCompressedFlags(HardwareInfo &hwInfo) const override;
    bool isCompressionForbidden(const HardwareInfo &hwInfo) const override;
    bool isExposingSubdevicesAllowed() const override;
    bool useAdditionalBlitProperties() const override;
    bool isNonCoherentTimestampsModeEnabled() const override;
    bool getStorageInfoLocalOnlyFlag(LocalMemAllocationMode usmDeviceAllocationMode, bool defaultValue) const override;
    bool isPidFdOrSocketForIpcSupported() const override;
    void adjustRTDispatchGlobals(RTDispatchGlobals &rtDispatchGlobals, const HardwareInfo &hwInfo) const override;
    uint32_t getSyncNumRTStacksPerDss(const HardwareInfo &hwInfo) const override;
    uint32_t getNumRtStacksPerDSSForAllocation(const HardwareInfo &hwInfo) const override;

    ~ProductHelperHw() override = default;

  protected:
    ProductHelperHw() = default;

    void enableBlitterOperationsSupport(HardwareInfo *hwInfo) const;
    bool getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities capability) const;
    uint64_t getHostMemCapabilitiesValue() const;
    bool getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) const;
    LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const override;
    void fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) const override;
    bool isCompressionForbiddenCommon(bool defaultValue) const;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableProductHelper {
    EnableProductHelper() {
        auto productHelperCreateFunction = ProductHelperHw<gfxProduct>::create;
        productHelperFactory[gfxProduct] = productHelperCreateFunction;
    }
};

} // namespace NEO

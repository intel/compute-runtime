/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/task_count_helper.h"

#include "aubstream/engine_node.h"

#include <igfxfmid.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aub_stream {
enum class ProductFamily : uint32_t;
}

namespace NEO {
struct KmdNotifyProperties;
struct AllocationData;
class CommandStreamReceiver;
class Device;
enum class LocalMemoryAccessMode;
struct FrontEndPropertiesSupport;
struct HardwareInfo;
struct KernelDescriptor;
struct PipelineSelectArgs;
struct PipelineSelectPropertiesSupport;
struct StateBaseAddressPropertiesSupport;
struct StateComputeModeProperties;
struct StateComputeModePropertiesSupport;
class ProductHelper;
class ReleaseHelper;
class Image;
class GraphicsAllocation;
class MemoryManager;
struct RootDeviceEnvironment;
struct TimeoutParams;
class OSInterface;
class DriverModel;
enum class DriverModelType;
enum class EngineGroupType : uint32_t;
enum class GfxMemoryAllocationMethod : uint32_t;
enum class AllocationType;
enum class CacheRegion : uint16_t;
enum class CachePolicy : uint32_t;

using ProductHelperCreateFunctionType = std::unique_ptr<ProductHelper> (*)();
extern ProductHelperCreateFunctionType productHelperFactory[IGFX_MAX_PRODUCT];

enum class UsmAccessCapabilities {
    host = 0,
    device,
    sharedSingleDevice,
    sharedCrossDevice,
    sharedSystemCrossDevice
};

class ProductHelper {
  public:
    static std::unique_ptr<ProductHelper> create(PRODUCT_FAMILY product) {
        auto productHelperCreateFunction = productHelperFactory[product];
        if (productHelperCreateFunction == nullptr) {
            return nullptr;
        }
        auto productHelper = productHelperCreateFunction();
        return productHelper;
    }

    static constexpr uint32_t uuidSize = 16u;
    static constexpr uint32_t luidSize = 8u;
    MOCKABLE_VIRTUAL int configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment);
    int configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment);
    virtual int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const = 0;
    virtual void adjustPlatformForProductFamily(HardwareInfo *hwInfo) = 0;
    virtual void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const = 0;
    virtual uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) const = 0;
    virtual uint64_t getDeviceMemCapabilities() const = 0;
    virtual uint64_t getSingleDeviceSharedMemCapabilities() const = 0;
    virtual uint64_t getCrossDeviceSharedMemCapabilities() const = 0;
    virtual uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const = 0;
    virtual std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() const = 0;
    virtual uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const = 0;
    virtual uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const = 0;
    virtual uint64_t getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const = 0;
    virtual bool isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const = 0;
    virtual uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const = 0;
    virtual void setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) const = 0;
    virtual void updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) const = 0;
    virtual bool obtainBlitterPreference(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBlitterFullySupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual std::optional<aub_stream::ProductFamily> getAubStreamProductFamily() const = 0;
    virtual bool isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool overrideGfxPartitionLayoutForWsl() const = 0;
    virtual bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool allowCompression(const HardwareInfo &hwInfo) const = 0;
    virtual LocalMemoryAccessMode getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;
    virtual bool isNewResidencyModelSupported() const = 0;
    virtual bool isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const = 0;
    virtual bool isDirectSubmissionConstantCacheInvalidationNeeded(const HardwareInfo &hwInfo) const = 0;
    virtual bool restartDirectSubmissionForHostptrFree() const = 0;
    virtual bool isAdjustDirectSubmissionTimeoutOnThrottleAndAcLineStatusEnabled() const = 0;
    virtual TimeoutParams getDirectSubmissionControllerTimeoutParams(bool acLineConnected, QueueThrottle queueThrottle) const = 0;
    virtual std::pair<bool, bool> isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs, const ReleaseHelper *releaseHelper) const = 0;
    virtual bool heapInLocalMem(const HardwareInfo &hwInfo) const = 0;
    virtual void setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const = 0;
    virtual bool isAdditionalMediaSamplerProgrammingRequired() const = 0;
    virtual bool isInitialFlagsProgrammingRequired() const = 0;
    virtual bool isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const = 0;
    virtual bool pipeControlWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool is3DPipelineSelectWARequired() const = 0;
    virtual bool isStorageInfoAdjustmentRequired() const = 0;
    virtual bool isBlitterForImagesSupported() const = 0;
    virtual bool isPageFaultSupported() const = 0;
    virtual bool isKmdMigrationSupported() const = 0;
    virtual bool isDisableScratchPagesSupported() const = 0;
    virtual bool isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isDcFlushAllowed() const = 0;
    virtual bool isDcFlushMitigated() const = 0;
    virtual bool mitigateDcFlush() const = 0;
    virtual bool overrideUsageForDcFlushMitigation(AllocationType allocationType) const = 0;
    virtual bool overridePatToUCAndTwoWayCohForDcFlushMitigation(AllocationType allocationType) const = 0;
    virtual bool overridePatToUCAndOneWayCohForDcFlushMitigation(AllocationType allocationType) const = 0;
    virtual bool overrideCacheableForDcFlushMitigation(AllocationType allocationType) const = 0;
    virtual uint32_t computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const = 0;
    virtual bool getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const = 0;
    virtual bool isFlushTaskAllowed() const = 0;
    virtual bool isSystolicModeConfigurable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const = 0;
    virtual size_t getSvmCpuAlignment() const = 0;
    virtual bool isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isVmBindPatIndexProgrammingSupported() const = 0;
    virtual bool isIpSamplingSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGrfNumReportedWithScm() const = 0;
    virtual bool isThreadArbitrationPolicyReportedWithScm() const = 0;
    virtual bool isCopyBufferRectSplitSupported() const = 0;
    virtual bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isTimestampWaitSupportedForEvents() const = 0;
    virtual bool isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool allowMemoryPrefetch(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBcsReportWaRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBlitSplitEnqueueWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBlitCopyRequiredForLocalMemory(const RootDeviceEnvironment &rootDeviceEnvironment, const GraphicsAllocation &allocation) const = 0;
    virtual bool isInitDeviceWithFirstSubmissionRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isImplicitScalingSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const = 0;
    virtual bool isUnlockingLockedPtrNecessary(const HardwareInfo &hwInfo) const = 0;
    virtual bool isAdjustWalkOrderAvailable(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool isAssignEngineRoundRobinSupported() const = 0;
    virtual uint32_t getL1CachePolicy(bool isDebuggerActive) const = 0;
    virtual bool isEvictionIfNecessaryFlagSupported() const = 0;
    virtual void adjustNumberOfCcs(HardwareInfo &hwInfo) const = 0;
    virtual bool blitEnqueuePreferred(bool isWriteToImageFromBuffer) const = 0;
    virtual bool isPrefetcherDisablingInDirectSubmissionRequired() const = 0;
    virtual bool isStatefulAddressingModeSupported() const = 0;
    virtual bool isPlatformQuerySupported() const = 0;
    virtual bool isNonBlockingGpuSubmissionSupported() const = 0;
    virtual bool isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const = 0;
    virtual bool isBufferPoolAllocatorSupported() const = 0;
    virtual bool isUsmPoolAllocatorSupported() const = 0;
    virtual bool isDeviceUsmAllocationReuseSupported() const = 0;
    virtual bool isHostUsmAllocationReuseSupported() const = 0;
    virtual bool useLocalPreferredForCacheableBuffers() const = 0;
    virtual bool useGemCreateExtInAllocateMemoryByKMD() const = 0;
    virtual bool isTlbFlushRequired() const = 0;
    virtual bool isDetectIndirectAccessInKernelSupported(const KernelDescriptor &kernelDescriptor, const bool isPrecompiled, const uint32_t precompiledKernelIndirectDetectionVersion) const = 0;
    virtual uint32_t getRequiredDetectIndirectVersion() const = 0;
    virtual uint32_t getRequiredDetectIndirectVersionVC() const = 0;
    virtual bool isLinearStoragePreferred(bool isImage1d, bool forceLinearStorage) const = 0;
    virtual bool isTranslationExceptionSupported() const = 0;
    virtual uint32_t getMaxNumSamplers() const = 0;
    virtual uint32_t getCommandBuffersPreallocatedPerCommandQueue() const = 0;
    virtual uint32_t getInternalHeapsPreallocated() const = 0;
    virtual bool overrideAllocationCacheable(const AllocationData &allocationData) const = 0;

    virtual bool getFrontEndPropertyScratchSizeSupport() const = 0;
    virtual bool getFrontEndPropertyPrivateScratchSizeSupport() const = 0;
    virtual bool getFrontEndPropertyComputeDispatchAllWalkerSupport() const = 0;
    virtual bool getFrontEndPropertyDisableEuFusionSupport() const = 0;
    virtual bool getFrontEndPropertyDisableOverDispatchSupport() const = 0;
    virtual bool getFrontEndPropertySingleSliceDispatchCcsModeSupport() const = 0;

    virtual bool getScmPropertyThreadArbitrationPolicySupport() const = 0;
    virtual bool getScmPropertyCoherencyRequiredSupport() const = 0;
    virtual bool getScmPropertyZPassAsyncComputeThreadLimitSupport() const = 0;
    virtual bool getScmPropertyPixelAsyncComputeThreadLimitSupport() const = 0;
    virtual bool getScmPropertyLargeGrfModeSupport() const = 0;
    virtual bool getScmPropertyDevicePreemptionModeSupport() const = 0;

    virtual bool getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport() const = 0;

    virtual bool getPreemptionDbgPropertyPreemptionModeSupport() const = 0;
    virtual bool getPreemptionDbgPropertyStateSipSupport() const = 0;
    virtual bool getPreemptionDbgPropertyCsrSurfaceSupport() const = 0;

    virtual bool getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const = 0;
    virtual bool getPipelineSelectPropertySystolicModeSupport() const = 0;

    virtual void fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) const = 0;
    virtual void fillScmPropertiesSupportStructureExtra(StateComputeModePropertiesSupport &propertiesSupport, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual void fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const = 0;
    virtual void fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const = 0;
    virtual void fillStateBaseAddressPropertiesSupportStructure(StateBaseAddressPropertiesSupport &propertiesSupport) const = 0;

    virtual bool isFusedEuDisabledForDpas(bool kernelHasDpasInstructions, const uint32_t *lws, const uint32_t *groupCount, const HardwareInfo &hwInfo) const = 0;
    virtual bool isCalculationForDisablingEuFusionWithDpasNeeded(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getNumberOfPartsInTileForConcurrentKernel(uint32_t ccsCount) const = 0;
    virtual bool is48bResourceNeededForRayTracing() const = 0;
    virtual bool disableL3CacheForDebug(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSkippingStatefulInformationRequired(const KernelDescriptor &kernelDescriptor) const = 0;
    virtual bool isResolvingSubDeviceIDNeeded(const ReleaseHelper *releaseHelper) const = 0;
    virtual uint64_t overridePatIndex(bool isUncachedType, uint64_t patIndex, AllocationType allocationType) const = 0;
    virtual std::vector<uint32_t> getSupportedNumGrfs(const ReleaseHelper *releaseHelper) const = 0;
    virtual aub_stream::EngineType getDefaultCopyEngine() const = 0;
    virtual void adjustEngineGroupType(EngineGroupType &engineGroupType) const = 0;
    virtual std::optional<GfxMemoryAllocationMethod> getPreferredAllocationMethod(AllocationType allocationType) const = 0;
    virtual bool isCachingOnCpuAvailable() const = 0;
    virtual bool isNewCoherencyModelSupported() const = 0;
    virtual bool deferMOCSToPatIndex() const = 0;
    virtual const std::vector<uint32_t> getSupportedLocalDispatchSizes(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getMaxLocalRegionSize(const HardwareInfo &hwInfo) const = 0;
    virtual bool localDispatchSizeQuerySupported() const = 0;
    virtual bool supportReadOnlyAllocations() const = 0;
    virtual bool isDeviceToHostCopySignalingFenceRequired() const = 0;
    virtual size_t getMaxFillPaternSizeForCopyEngine() const = 0;
    virtual bool isAvailableExtendedScratch() const = 0;
    virtual std::optional<bool> isCoherentAllocation(uint64_t patIndex) const = 0;
    virtual bool isStagingBuffersEnabled() const = 0;
    virtual uint32_t getCacheLineSize() const = 0;
    virtual bool supports2DBlockStore() const = 0;
    virtual bool supports2DBlockLoad() const = 0;
    virtual uint32_t getNumCacheRegions() const = 0;
    virtual uint64_t getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const = 0;

    virtual ~ProductHelper() = default;

  protected:
    ProductHelper() = default;

    virtual LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;
    virtual void fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) const = 0;
    void setupDefaultEngineType(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) const;
    int setupProductSpecificConfig(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) const;
    static void setupPreemptionSurfaceSize(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void setupKmdNotifyProperties(KmdNotifyProperties &kmdNotifyProperties);
    static void setupPreemptionMode(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment, bool kmdPreemptionSupport);
    static void setupImageSupport(HardwareInfo &hwInfo);
};
} // namespace NEO

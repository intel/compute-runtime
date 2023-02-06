/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <igfxfmid.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t;
}

namespace aub_stream {
enum class ProductFamily : uint32_t;
}

namespace NEO {
class Device;
enum class LocalMemoryAccessMode;
struct FrontEndPropertiesSupport;
struct HardwareInfo;
struct PipelineSelectArgs;
struct PipelineSelectPropertiesSupport;
struct StateBaseAddressPropertiesSupport;
struct StateComputeModeProperties;
struct StateComputeModePropertiesSupport;
class ProductHelper;
class GraphicsAllocation;
class MemoryManager;
struct RootDeviceEnvironment;
class OSInterface;
enum class DriverModelType;
enum class AllocationType;

using ProductHelperCreateFunctionType = std::unique_ptr<ProductHelper> (*)();
extern ProductHelperCreateFunctionType productHelperFactory[IGFX_MAX_PRODUCT];

enum class UsmAccessCapabilities {
    Host = 0,
    Device,
    SharedSingleDevice,
    SharedCrossDevice,
    SharedSystemCrossDevice
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
    int configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment);
    int configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment);
    virtual int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const = 0;
    virtual void adjustPlatformForProductFamily(HardwareInfo *hwInfo) = 0;
    virtual void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const = 0;
    virtual void disableRcsExposure(HardwareInfo *hwInfo) const = 0;
    virtual uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) const = 0;
    virtual uint64_t getDeviceMemCapabilities() const = 0;
    virtual uint64_t getSingleDeviceSharedMemCapabilities() const = 0;
    virtual uint64_t getCrossDeviceSharedMemCapabilities() const = 0;
    virtual uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const = 0;
    virtual void getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) const = 0;
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
    virtual void updateIddCommand(void *const commandPtr, uint32_t numGrf, int32_t threadArbitrationPolicy) const = 0;
    virtual bool obtainBlitterPreference(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBlitterFullySupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual AOT::PRODUCT_CONFIG getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual std::optional<aub_stream::ProductFamily> getAubStreamProductFamily() const = 0;
    virtual bool isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool overrideGfxPartitionLayoutForWsl() const = 0;
    virtual std::string getDeviceMemoryName() const = 0;
    virtual bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool allowCompression(const HardwareInfo &hwInfo) const = 0;
    virtual LocalMemoryAccessMode getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;
    virtual bool isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isNewResidencyModelSupported() const = 0;
    virtual bool isDirectSubmissionSupported(const HardwareInfo &hwInfo) const = 0;
    virtual std::pair<bool, bool> isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const = 0;
    virtual bool heapInLocalMem(const HardwareInfo &hwInfo) const = 0;
    virtual void setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) = 0;
    virtual bool isAdditionalMediaSamplerProgrammingRequired() const = 0;
    virtual bool isInitialFlagsProgrammingRequired() const = 0;
    virtual bool isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const = 0;
    virtual bool extraParametersInvalid(const HardwareInfo &hwInfo) const = 0;
    virtual bool pipeControlWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool is3DPipelineSelectWARequired() const = 0;
    virtual bool isStorageInfoAdjustmentRequired() const = 0;
    virtual bool isBlitterForImagesSupported() const = 0;
    virtual bool isPageFaultSupported() const = 0;
    virtual bool isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isDcFlushAllowed() const = 0;
    virtual uint32_t computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const = 0;
    virtual bool getUuid(Device *device, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const = 0;
    virtual bool isFlushTaskAllowed() const = 0;
    virtual bool programAllStateComputeCommandFields() const = 0;
    virtual bool isSystolicModeConfigurable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const = 0;
    virtual size_t getSvmCpuAlignment() const = 0;
    virtual bool isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isVmBindPatIndexProgrammingSupported() const = 0;
    virtual bool isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isIpSamplingSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGrfNumReportedWithScm() const = 0;
    virtual bool isThreadArbitrationPolicyReportedWithScm() const = 0;
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
    virtual bool isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isAssignEngineRoundRobinSupported() const = 0;
    virtual uint32_t getL1CachePolicy(bool isDebuggerActive) const = 0;
    virtual bool isEvictionIfNecessaryFlagSupported() const = 0;
    virtual void adjustNumberOfCcs(HardwareInfo &hwInfo) const = 0;
    virtual bool isPrefetcherDisablingInDirectSubmissionRequired() const = 0;
    virtual bool isStatefulAddressingModeSupported() const = 0;
    virtual bool isPlatformQuerySupported() const = 0;
    virtual bool isNonBlockingGpuSubmissionSupported() const = 0;
    virtual bool isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ) const = 0;
    virtual bool isMidThreadPreemptionDisallowedForRayTracingKernels() const = 0;
    virtual bool isBufferPoolAllocatorSupported() const = 0;
    virtual uint64_t overridePatIndex(AllocationType allocationType, uint64_t patIndex) const = 0;
    virtual bool isTlbFlushRequired() const = 0;
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

    virtual bool getStateBaseAddressPropertyGlobalAtomicsSupport() const = 0;
    virtual bool getStateBaseAddressPropertyStatelessMocsSupport() const = 0;
    virtual bool getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport() const = 0;

    virtual bool getPreemptionDbgPropertyPreemptionModeSupport() const = 0;
    virtual bool getPreemptionDbgPropertyStateSipSupport() const = 0;
    virtual bool getPreemptionDbgPropertyCsrSurfaceSupport() const = 0;

    virtual bool getPipelineSelectPropertyModeSelectedSupport() const = 0;
    virtual bool getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const = 0;
    virtual bool getPipelineSelectPropertySystolicModeSupport() const = 0;

    virtual void fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) const = 0;
    virtual void fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const = 0;
    virtual void fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) const = 0;
    virtual void fillStateBaseAddressPropertiesSupportStructure(StateBaseAddressPropertiesSupport &propertiesSupport) const = 0;
    virtual uint32_t getDefaultRevisionId() const = 0;

    virtual bool isMultiContextResourceDeferDeletionSupported() const = 0;

    virtual ~ProductHelper() = default;

  protected:
    ProductHelper() = default;

    virtual LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;
    virtual void fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) const = 0;

  public:
    uint32_t threadsPerEu = 0u;
};

} // namespace NEO

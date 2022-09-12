/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/local_memory_access_modes.h"

#include "igfxfmid.h"

namespace AOT {
enum PRODUCT_CONFIG : uint32_t;
}

namespace NEO {

struct FrontEndPropertiesSupport;
struct HardwareInfo;
struct PipelineSelectArgs;
struct PipelineSelectPropertiesSupport;
struct StateComputeModeProperties;
struct StateComputeModePropertiesSupport;
class HwInfoConfig;
class GraphicsAllocation;
class MemoryManager;
class OSInterface;
enum class DriverModelType;

extern HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT];

enum class UsmAccessCapabilities {
    Host = 0,
    Device,
    SharedSingleDevice,
    SharedCrossDevice,
    SharedSystemCrossDevice
};

class HwInfoConfig {
  public:
    static HwInfoConfig *get(PRODUCT_FAMILY product) {
        return hwInfoConfigFactory[product];
    }
    static constexpr uint32_t uuidSize = 16u;
    static constexpr uint32_t luidSize = 8u;
    int configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface);
    int configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface);
    virtual int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) = 0;
    virtual void adjustPlatformForProductFamily(HardwareInfo *hwInfo) = 0;
    virtual void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) = 0;
    virtual uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) = 0;
    virtual uint64_t getDeviceMemCapabilities() = 0;
    virtual uint64_t getSingleDeviceSharedMemCapabilities() = 0;
    virtual uint64_t getCrossDeviceSharedMemCapabilities() = 0;
    virtual uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) = 0;
    virtual void getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) = 0;
    virtual std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() = 0;
    virtual void convertTimestampsFromOaToCsDomain(uint64_t &timestampData) = 0;
    virtual uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) = 0;
    virtual uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) = 0;
    virtual uint64_t getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) = 0;
    virtual bool isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const = 0;
    virtual uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const = 0;
    virtual void setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) = 0;
    virtual void updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) = 0;
    virtual void updateIddCommand(void *const commandPtr, uint32_t numGrf, int32_t threadArbitrationPolicy) = 0;
    virtual bool obtainBlitterPreference(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBlitterFullySupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual AOT::PRODUCT_CONFIG getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual bool isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool overrideGfxPartitionLayoutForWsl() const = 0;
    virtual std::string getDeviceMemoryName() const = 0;
    virtual bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool allowCompression(const HardwareInfo &hwInfo) const = 0;
    virtual bool allowStatelessCompression(const HardwareInfo &hwInfo) const = 0;
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
    virtual bool isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isDcFlushAllowed() const = 0;
    virtual uint32_t computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const = 0;
    virtual bool getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const = 0;
    virtual bool isFlushTaskAllowed() const = 0;
    virtual bool programAllStateComputeCommandFields() const = 0;
    virtual bool isSystolicModeConfigurable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const = 0;
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
    virtual bool isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const = 0;
    virtual bool isImplicitScalingSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const = 0;
    virtual bool isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isAssignEngineRoundRobinSupported() const = 0;
    virtual uint32_t getL1CachePolicy(bool isDebuggerActive) const = 0;
    virtual bool isEvictionIfNecessaryFlagSupported() const = 0;
    virtual void adjustNumberOfCcs(HardwareInfo &hwInfo) const = 0;
    virtual bool isPrefetcherDisablingInDirectSubmissionRequired() const = 0;
    virtual bool isStatefulAddressingModeSupported() const = 0;
    virtual bool isPlatformQuerySupported() const = 0;

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

    virtual bool getSbaPropertyGlobalAtomicsSupport() const = 0;
    virtual bool getSbaPropertyStatelessMocsSupport() const = 0;

    virtual bool getPreemptionDbgPropertyPreemptionModeSupport() const = 0;
    virtual bool getPreemptionDbgPropertyStateSipSupport() const = 0;
    virtual bool getPreemptionDbgPropertyCsrSurfaceSupport() const = 0;

    virtual bool getPipelineSelectPropertyModeSelectedSupport() const = 0;
    virtual bool getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const = 0;
    virtual bool getPipelineSelectPropertySystolicModeSupport() const = 0;

    virtual void fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) = 0;
    virtual void fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) = 0;
    virtual void fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) = 0;

    MOCKABLE_VIRTUAL ~HwInfoConfig() = default;

  protected:
    virtual LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;
    virtual void fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) = 0;

  public:
    uint32_t threadsPerEu = 0u;
};

template <PRODUCT_FAMILY gfxProduct>
class HwInfoConfigHw : public HwInfoConfig {
  public:
    static HwInfoConfig *get() {
        static HwInfoConfigHw<gfxProduct> instance;
        return &instance;
    }
    int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) override;
    void adjustPlatformForProductFamily(HardwareInfo *hwInfo) override;
    void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) override;
    uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) override;
    uint64_t getDeviceMemCapabilities() override;
    uint64_t getSingleDeviceSharedMemCapabilities() override;
    uint64_t getCrossDeviceSharedMemCapabilities() override;
    uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) override;
    void getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) override;
    std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() override;
    void convertTimestampsFromOaToCsDomain(uint64_t &timestampData) override;
    uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) override;
    uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) override;
    uint64_t getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) override;
    bool isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const override;
    bool isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const override;
    uint32_t getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const override;
    uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const override;
    void setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) override;
    void updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) override;
    void updateIddCommand(void *const commandPtr, uint32_t numGrf, int32_t threadArbitrationPolicy) override;
    bool obtainBlitterPreference(const HardwareInfo &hwInfo) const override;
    bool isBlitterFullySupported(const HardwareInfo &hwInfo) const override;
    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override;
    bool overrideGfxPartitionLayoutForWsl() const override;
    uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const override;
    AOT::PRODUCT_CONFIG getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const override;
    uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    bool isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const override;
    std::string getDeviceMemoryName() const override;
    bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const override;
    bool allowCompression(const HardwareInfo &hwInfo) const override;
    bool allowStatelessCompression(const HardwareInfo &hwInfo) const override;
    LocalMemoryAccessMode getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const override;
    bool isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const override;
    bool isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const override;
    bool isNewResidencyModelSupported() const override;
    bool isDirectSubmissionSupported(const HardwareInfo &hwInfo) const override;
    std::pair<bool, bool> isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const override;
    bool heapInLocalMem(const HardwareInfo &hwInfo) const override;
    void setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) override;
    bool isAdditionalMediaSamplerProgrammingRequired() const override;
    bool isInitialFlagsProgrammingRequired() const override;
    bool isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const override;
    bool extraParametersInvalid(const HardwareInfo &hwInfo) const override;
    bool pipeControlWARequired(const HardwareInfo &hwInfo) const override;
    bool imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const override;
    bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const override;
    bool is3DPipelineSelectWARequired() const override;
    bool isStorageInfoAdjustmentRequired() const override;
    bool isBlitterForImagesSupported() const override;
    bool isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const override;
    bool isDcFlushAllowed() const override;
    uint32_t computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const override;
    bool getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const override;
    bool isFlushTaskAllowed() const override;
    bool programAllStateComputeCommandFields() const override;
    bool isSystolicModeConfigurable(const HardwareInfo &hwInfo) const override;
    bool isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const override;
    bool isCopyEngineSelectorEnabled(const HardwareInfo &hwInfo) const override;
    bool isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const override;
    bool isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const override;
    bool isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const override;
    uint32_t getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const override;
    bool isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const override;
    bool isVmBindPatIndexProgrammingSupported() const override;
    bool isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const override;
    bool isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const override;
    bool isIpSamplingSupported(const HardwareInfo &hwInfo) const override;
    bool isGrfNumReportedWithScm() const override;
    bool isThreadArbitrationPolicyReportedWithScm() const override;
    bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override;
    bool isTimestampWaitSupportedForEvents() const override;
    bool isTilePlacementResourceWaRequired(const HardwareInfo &hwInfo) const override;
    bool isBlitSplitEnqueueWARequired(const HardwareInfo &hwInfo) const override;
    bool allowMemoryPrefetch(const HardwareInfo &hwInfo) const override;
    bool isBcsReportWaRequired(const HardwareInfo &hwInfo) const override;
    bool isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const override;
    bool isImplicitScalingSupported(const HardwareInfo &hwInfo) const override;
    bool isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const override;
    bool isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const override;
    bool isAssignEngineRoundRobinSupported() const override;
    uint32_t getL1CachePolicy(bool isDebuggerActive) const override;
    bool isEvictionIfNecessaryFlagSupported() const override;
    void adjustNumberOfCcs(HardwareInfo &hwInfo) const override;
    bool isPrefetcherDisablingInDirectSubmissionRequired() const override;
    bool isStatefulAddressingModeSupported() const override;
    bool isPlatformQuerySupported() const override;

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

    bool getSbaPropertyGlobalAtomicsSupport() const override;
    bool getSbaPropertyStatelessMocsSupport() const override;

    bool getPreemptionDbgPropertyPreemptionModeSupport() const override;
    bool getPreemptionDbgPropertyStateSipSupport() const override;
    bool getPreemptionDbgPropertyCsrSurfaceSupport() const override;

    bool getPipelineSelectPropertyModeSelectedSupport() const override;
    bool getPipelineSelectPropertyMediaSamplerDopClockGateSupport() const override;
    bool getPipelineSelectPropertySystolicModeSupport() const override;

    void fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) override;
    void fillFrontEndPropertiesSupportStructure(FrontEndPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) override;

  protected:
    HwInfoConfigHw() = default;

    void enableCompression(HardwareInfo *hwInfo);
    void enableBlitterOperationsSupport(HardwareInfo *hwInfo);
    bool getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities capability);
    uint64_t getHostMemCapabilitiesValue();
    bool getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo);
    LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const override;
    void fillScmPropertiesSupportStructureBase(StateComputeModePropertiesSupport &propertiesSupport) override;
    void fillPipelineSelectPropertiesSupportStructure(PipelineSelectPropertiesSupport &propertiesSupport, const HardwareInfo &hwInfo) override;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableProductHwInfoConfig {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;

    EnableProductHwInfoConfig() {
        HwInfoConfig *pHwInfoConfig = HwInfoConfigHw<gfxProduct>::get();
        hwInfoConfigFactory[gfxProduct] = pHwInfoConfig;
        pHwInfoConfig->threadsPerEu = GfxProduct::threadsPerEu;
    }
};

} // namespace NEO

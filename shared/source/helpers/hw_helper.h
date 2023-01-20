/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/options.h"

#include "aubstream/aubstream.h"
#include "igfxfmid.h"

#include <cstdint>
#include <memory>
#include <string>

namespace AubMemDump {
struct LrcaHelper;
}

namespace NEO {
enum class AuxTranslationMode;
struct FeatureTable;
enum class PostSyncMode : uint32_t;
enum class CachePolicy : uint32_t;
enum class CacheRegion : uint16_t;
class GmmHelper;
class GraphicsAllocation;
class TagAllocatorBase;
class LinearStream;
class Gmm;
class MemoryManager;
struct AllocationData;
struct AllocationProperties;
struct EncodeSurfaceStateArgs;
struct RootDeviceEnvironment;
struct PipeControlArgs;
class ProductHelper;

class GfxCoreHelper {
  public:
    static GfxCoreHelper &get(GFXCORE_FAMILY gfxCore);
    virtual size_t getMaxBarrierRegisterPerSlice() const = 0;
    virtual size_t getPaddingForISAAllocation() const = 0;
    virtual uint32_t getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual uint32_t getPitchAlignmentForImage(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual uint32_t getMaxNumSamplers() const = 0;
    virtual void adjustDefaultEngineType(HardwareInfo *pHwInfo) = 0;
    virtual SipKernelType getSipKernelType(bool debuggingActive) const = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const = 0;
    virtual bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const = 0;
    virtual bool hvAlign4Required() const = 0;
    virtual bool preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const = 0;
    virtual bool isBufferSizeSuitableForCompression(const size_t size) const = 0;
    virtual bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) const = 0;
    static bool compressedBuffersSupported(const HardwareInfo &hwInfo);
    static bool compressedImagesSupported(const HardwareInfo &hwInfo);
    static bool cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo);
    static uint32_t getHighestEnabledSlice(const HardwareInfo &hwInfo);
    virtual bool timestampPacketWriteSupported() const = 0;
    virtual bool isTimestampWaitSupportedForQueues() const = 0;
    virtual bool isUpdateTaskCountFromWaitSupported() const = 0;
    virtual size_t getRenderSurfaceStateSize() const = 0;
    virtual void setRenderSurfaceStateForScratchResource(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                         void *surfaceStateBuffer,
                                                         size_t bufferSize,
                                                         uint64_t gpuVa,
                                                         size_t offset,
                                                         uint32_t pitch,
                                                         GraphicsAllocation *gfxAlloc,
                                                         bool isReadOnly,
                                                         uint32_t surfaceType,
                                                         bool forceNonAuxMode,
                                                         bool useL1Cache) const = 0;
    virtual const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const = 0;
    virtual EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const = 0;
    virtual const StackVec<size_t, 3> getDeviceSubGroupSizes() const = 0;
    virtual const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const = 0;
    virtual bool getEnableLocalMemory(const HardwareInfo &hwInfo) const = 0;
    virtual std::string getExtensions(const HardwareInfo &hwInfo) const = 0;
    static uint32_t getMaxThreadsForVfe(const HardwareInfo &hwInfo);
    virtual uint32_t getMetricsLibraryGenId() const = 0;
    virtual uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const = 0;
    virtual bool isLinearStoragePreferred(bool isSharedContext, bool isImage1d, bool forceLinearStorage) const = 0;
    virtual uint8_t getBarriersCountFromHasBarriers(uint8_t hasBarriers) const = 0;
    virtual uint32_t calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const = 0;
    virtual uint32_t alignSlmSize(uint32_t slmSize) const = 0;
    virtual uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const = 0;

    virtual bool isWaDisableRccRhwoOptimizationRequired() const = 0;
    virtual bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const = 0;
    virtual uint32_t getMinimalSIMDSize() const = 0;
    virtual bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const = 0;
    virtual bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const = 0;
    virtual uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const = 0;
    virtual uint32_t getBindlessSurfaceExtendedMessageDescriptorValue(uint32_t surfStateOffset) const = 0;
    virtual void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const = 0;
    virtual bool isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const = 0;
    virtual uint32_t getGlobalTimeStampBits() const = 0;
    virtual int32_t getDefaultThreadArbitrationPolicy() const = 0;
    virtual bool useOnlyGlobalTimestamps() const = 0;
    virtual bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const = 0;
    virtual bool packedFormatsSupported() const = 0;
    virtual bool isAssignEngineRoundRobinSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isRcsAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                             const HardwareInfo &hwInfo, bool isEngineInstanced) const = 0;
    virtual size_t getMaxFillPaternSizeForCopyEngine() const = 0;
    virtual size_t getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSipWANeeded(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const = 0;
    virtual bool isKmdMigrationSupported(const HardwareInfo &hwInfo) const = 0;
    virtual aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const = 0;
    virtual uint32_t getNumCacheRegions() const = 0;
    virtual bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const = 0;
    virtual uint32_t getPlanarYuvMaxHeight() const = 0;
    virtual size_t getPreemptionAllocationAlignment() const = 0;
    virtual std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager,
                                                                             size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                             DeviceBitfield deviceBitfield) const = 0;
    virtual size_t getTimestampPacketAllocatorAlignment() const = 0;
    virtual size_t getSingleTimestampPacketSize() const = 0;
    virtual void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const = 0;
    virtual void applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const = 0;
    virtual bool unTypedDataPortCacheFlushRequired() const = 0;
    virtual bool isEngineTypeRemappingToHwSpecificRequired() const = 0;

    static uint32_t getSubDevicesCount(const HardwareInfo *pHwInfo);

    virtual bool isSipKernelAsHexadecimalArrayPreferred() const = 0;
    virtual void setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize) const = 0;
    virtual void adjustPreemptionSurfaceSize(size_t &csrSize) const = 0;
    virtual size_t getSamplerStateSize() const = 0;
    virtual bool preferInternalBcsEngine() const = 0;
    virtual bool isScratchSpaceSurfaceStateAccessible() const = 0;
    virtual uint32_t getMaxScratchSize() const = 0;
    virtual uint64_t getRenderSurfaceStateBaseAddress(void *renderSurfaceState) const = 0;
    virtual uint32_t getRenderSurfaceStatePitch(void *renderSurfaceState) const = 0;
    virtual size_t getMax3dImageWidthOrHeight() const = 0;
    virtual uint64_t getMaxMemAllocSize() const = 0;
    virtual uint64_t getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const = 0;
    virtual bool isStatelessToStatefulWithOffsetSupported() const = 0;
    virtual void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const = 0;
    virtual bool disableL3CacheForDebug(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const = 0;
    virtual bool isRevisionSpecificBinaryBuiltinRequired() const = 0;
    virtual bool forceNonGpuCoherencyWA(bool requiresCoherency) const = 0;
    virtual bool platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual size_t getBatchBufferEndSize() const = 0;
    virtual const void *getBatchBufferEndReference() const = 0;
    virtual bool isPlatformFlushTaskEnabled(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getMinimalScratchSpaceSize() const = 0;
    virtual bool copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getAmountOfAllocationsToFill() const = 0;
    virtual bool isChipsetUniqueUUIDSupported() const = 0;
    virtual bool isTimestampShiftRequired() const = 0;
    virtual bool isRelaxedOrderingSupported() const = 0;
    static bool isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo, const ProductHelper &productHelper);

  protected:
    GfxCoreHelper() = default;
};

template <typename GfxFamily>
class GfxCoreHelperHw : public GfxCoreHelper {
  public:
    static GfxCoreHelperHw<GfxFamily> &get() {
        static GfxCoreHelperHw<GfxFamily> gfxCoreHelper;
        return gfxCoreHelper;
    }

    size_t getRenderSurfaceStateSize() const override {
        using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
        return sizeof(RENDER_SURFACE_STATE);
    }

    size_t getSamplerStateSize() const override {
        using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
        return sizeof(SAMPLER_STATE);
    }

    uint32_t getBindlessSurfaceExtendedMessageDescriptorValue(uint32_t surfStateOffset) const override {
        using DataPortBindlessSurfaceExtendedMessageDescriptor = typename GfxFamily::DataPortBindlessSurfaceExtendedMessageDescriptor;
        DataPortBindlessSurfaceExtendedMessageDescriptor messageExtDescriptor = {};
        messageExtDescriptor.setBindlessSurfaceOffset(surfStateOffset);
        return messageExtDescriptor.getBindlessSurfaceOffsetToPatch();
    }

    uint64_t getRenderSurfaceStateBaseAddress(void *renderSurfaceState) const override {
        return reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(renderSurfaceState)->getSurfaceBaseAddress();
    }

    uint32_t getRenderSurfaceStatePitch(void *renderSurfaceState) const override {
        return reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(renderSurfaceState)->getSurfacePitch();
    }

    const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const override;

    size_t getMaxBarrierRegisterPerSlice() const override;

    size_t getPaddingForISAAllocation() const override;

    uint32_t getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    uint32_t getPitchAlignmentForImage(const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    uint32_t getMaxNumSamplers() const override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo) override;

    SipKernelType getSipKernelType(bool debuggingActive) const override;

    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override;

    bool hvAlign4Required() const override;

    bool isBufferSizeSuitableForCompression(const size_t size) const override;

    bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) const override;

    bool timestampPacketWriteSupported() const override;

    bool isTimestampWaitSupportedForQueues() const override;
    bool isUpdateTaskCountFromWaitSupported() const override;

    bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const override;

    bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const override;

    void setRenderSurfaceStateForScratchResource(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                 void *surfaceStateBuffer,
                                                 size_t bufferSize,
                                                 uint64_t gpuVa,
                                                 size_t offset,
                                                 uint32_t pitch,
                                                 GraphicsAllocation *gfxAlloc,
                                                 bool isReadOnly,
                                                 uint32_t surfaceType,
                                                 bool forceNonAuxMode,
                                                 bool useL1Cache) const override;

    MOCKABLE_VIRTUAL void setL1CachePolicy(bool useL1Cache, typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) const;

    const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const override;

    EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override;

    const StackVec<size_t, 3> getDeviceSubGroupSizes() const override;

    const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const override;

    bool getEnableLocalMemory(const HardwareInfo &hwInfo) const override;

    std::string getExtensions(const HardwareInfo &hwInfo) const override;

    uint32_t getMetricsLibraryGenId() const override;

    uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const override;

    bool isLinearStoragePreferred(bool isSharedContext, bool isImage1d, bool forceLinearStorage) const override;

    uint8_t getBarriersCountFromHasBarriers(uint8_t hasBarriers) const override;

    uint32_t calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const override;

    uint32_t alignSlmSize(uint32_t slmSize) const override;

    uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const override;

    static AuxTranslationMode getAuxTranslationMode(const HardwareInfo &hwInfo);

    bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override;

    bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const override;

    static bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo);

    bool isWaDisableRccRhwoOptimizationRequired() const override;

    bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const override;

    uint32_t getMinimalSIMDSize() const override;

    uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const override;

    uint32_t getGlobalTimeStampBits() const override;

    void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const override;

    bool isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override;

    int32_t getDefaultThreadArbitrationPolicy() const override;

    bool useOnlyGlobalTimestamps() const override;

    bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const override;

    bool packedFormatsSupported() const override;

    bool isRcsAvailable(const HardwareInfo &hwInfo) const override;

    bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const override;

    uint32_t adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                     const HardwareInfo &hwInfo, bool isEngineInstanced) const override;

    size_t getMaxFillPaternSizeForCopyEngine() const override;

    size_t getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const override;

    bool isKmdMigrationSupported(const HardwareInfo &hwInfo) const override;

    bool isSipWANeeded(const HardwareInfo &hwInfo) const override;

    bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const override;

    aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const override;

    uint32_t getNumCacheRegions() const override;

    bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const override;

    uint32_t getPlanarYuvMaxHeight() const override;

    size_t getPreemptionAllocationAlignment() const override;

    std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager,
                                                                     size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                     DeviceBitfield deviceBitfield) const override;
    size_t getTimestampPacketAllocatorAlignment() const override;

    size_t getSingleTimestampPacketSize() const override;
    static size_t getSingleTimestampPacketSizeHw();

    void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const override;

    bool preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const override;

    void applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const override;

    bool unTypedDataPortCacheFlushRequired() const override;

    bool isAssignEngineRoundRobinSupported(const HardwareInfo &hwInfo) const override;

    bool isEngineTypeRemappingToHwSpecificRequired() const override;

    bool isSipKernelAsHexadecimalArrayPreferred() const override;

    void setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize) const override;

    void adjustPreemptionSurfaceSize(size_t &csrSize) const override;
    bool isScratchSpaceSurfaceStateAccessible() const override;
    uint32_t getMaxScratchSize() const override;
    bool preferInternalBcsEngine() const override;
    size_t getMax3dImageWidthOrHeight() const override;
    uint64_t getMaxMemAllocSize() const override;
    uint64_t getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const override;
    bool isStatelessToStatefulWithOffsetSupported() const override;
    void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override;
    bool disableL3CacheForDebug(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override;
    bool isRevisionSpecificBinaryBuiltinRequired() const override;
    bool forceNonGpuCoherencyWA(bool requiresCoherency) const override;
    bool platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const override;
    size_t getBatchBufferEndSize() const override;
    const void *getBatchBufferEndReference() const override;
    bool isPlatformFlushTaskEnabled(const NEO::HardwareInfo &hwInfo) const override;
    uint32_t getMinimalScratchSpaceSize() const override;
    bool copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo) const override;
    uint32_t getAmountOfAllocationsToFill() const override;
    bool isChipsetUniqueUUIDSupported() const override;
    bool isTimestampShiftRequired() const override;
    bool isRelaxedOrderingSupported() const override;

  protected:
    static const AuxTranslationMode defaultAuxTranslationMode;
    GfxCoreHelperHw() = default;
};

struct DwordBuilder {
    static uint32_t build(uint32_t bitNumberToSet, bool masked, bool set = true, uint32_t initValue = 0) {
        uint32_t dword = initValue;
        if (set) {
            dword |= (1 << bitNumberToSet);
        }
        if (masked) {
            dword |= (1 << (bitNumberToSet + 16));
        }
        return dword;
    };
};

template <typename GfxFamily>
struct LriHelper {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    static void program(LinearStream *cmdStream, uint32_t address, uint32_t value, bool remap);
};

template <typename GfxFamily>
struct MemorySynchronizationCommands {
    static void addSingleBarrier(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args);
    static void setSingleBarrier(void *commandsBuffer, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args);
    static void addSingleBarrier(LinearStream &commandStream, PipeControlArgs &args);
    static void setSingleBarrier(void *commandsBuffer, PipeControlArgs &args);

    static void addBarrierWithPostSyncOperation(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, const HardwareInfo &hwInfo, PipeControlArgs &args);
    static void setBarrierWithPostSyncOperation(void *&commandsBuffer, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, const HardwareInfo &hwInfo, PipeControlArgs &args);

    static void setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo);

    static void addBarrierWa(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo);
    static void setBarrierWa(void *&commandsBuffer, uint64_t gpuAddress, const HardwareInfo &hwInfo);

    static void setBarrierWaFlags(void *barrierCmd);

    static void addAdditionalSynchronizationForDirectSubmission(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo);
    static void addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo);
    static void setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo);

    static bool getDcFlushEnable(bool isFlushPreferred, const HardwareInfo &hwInfo);

    static void addFullCacheFlush(LinearStream &commandStream, const HardwareInfo &hwInfo);
    static void setCacheFlushExtraProperties(PipeControlArgs &args);

    static size_t getSizeForBarrierWithPostSyncOperation(const HardwareInfo &hwInfo, bool tlbInvalidationRequired);
    static size_t getSizeForBarrierWa(const HardwareInfo &hwInfo);
    static size_t getSizeForSingleBarrier(bool tlbInvalidationRequired);
    static size_t getSizeForSingleAdditionalSynchronizationForDirectSubmission(const HardwareInfo &hwInfo);
    static size_t getSizeForSingleAdditionalSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForAdditonalSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForFullCacheFlush();

    static bool isBarrierWaRequired(const HardwareInfo &hwInfo);
    static bool isBarrierlPriorToPipelineSelectWaRequired(const HardwareInfo &hwInfo);
    static void setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args);
};

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : BITFIELD_RANGE(0, 6);
        uint32_t Height : BITFIELD_RANGE(7, 20);
        uint32_t Depth : BITFIELD_RANGE(21, 31);
    } SurfaceState;
};

} // namespace NEO

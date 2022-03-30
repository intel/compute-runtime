/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/options.h"
#include "shared/source/utilities/stackvec.h"

#include "hw_cmds.h"
#include "third_party/aub_stream/headers/aubstream.h"

#include <cstdint>
#include <string>
#include <type_traits>

namespace NEO {
class GmmHelper;
class GraphicsAllocation;
class TagAllocatorBase;
class LinearStream;
class Gmm;
struct AllocationData;
struct AllocationProperties;
struct EncodeSurfaceStateArgs;
struct EngineControl;
struct RootDeviceEnvironment;
struct PipeControlArgs;

class HwHelper {
  public:
    static HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getBindingTableStateSurfaceStatePointer(const void *pBindingTable, uint32_t index) = 0;
    virtual size_t getBindingTableStateSize() const = 0;
    virtual uint32_t getBindingTableStateAlignement() const = 0;
    virtual size_t getInterfaceDescriptorDataSize() const = 0;
    virtual size_t getMaxBarrierRegisterPerSlice() const = 0;
    virtual size_t getPaddingForISAAllocation() const = 0;
    virtual uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const = 0;
    virtual uint32_t getPitchAlignmentForImage(const HardwareInfo *hwInfo) const = 0;
    virtual uint32_t getMaxNumSamplers() const = 0;
    virtual void adjustDefaultEngineType(HardwareInfo *pHwInfo) = 0;
    virtual bool isL3Configurable(const HardwareInfo &hwInfo) = 0;
    virtual SipKernelType getSipKernelType(bool debuggingActive) const = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const = 0;
    virtual bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const = 0;
    virtual bool hvAlign4Required() const = 0;
    virtual bool preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const = 0;
    virtual bool isBufferSizeSuitableForCompression(const size_t size, const HardwareInfo &hwInfo) const = 0;
    virtual bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) = 0;
    virtual bool isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const = 0;
    static bool compressedBuffersSupported(const HardwareInfo &hwInfo);
    static bool compressedImagesSupported(const HardwareInfo &hwInfo);
    static bool cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo);
    virtual bool timestampPacketWriteSupported() const = 0;
    virtual bool isTimestampWaitSupported() const = 0;
    virtual bool isUpdateTaskCountFromWaitSupported() const = 0;
    virtual size_t getRenderSurfaceStateSize() const = 0;
    virtual void setRenderSurfaceStateForBuffer(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                void *surfaceStateBuffer,
                                                size_t bufferSize,
                                                uint64_t gpuVa,
                                                size_t offset,
                                                uint32_t pitch,
                                                GraphicsAllocation *gfxAlloc,
                                                bool isReadOnly,
                                                uint32_t surfaceType,
                                                bool forceNonAuxMode,
                                                bool useL1Cache) = 0;
    virtual const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const = 0;
    virtual EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const = 0;
    virtual const StackVec<size_t, 3> getDeviceSubGroupSizes() const = 0;
    virtual const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const = 0;
    virtual bool getEnableLocalMemory(const HardwareInfo &hwInfo) const = 0;
    virtual std::string getExtensions() const = 0;
    static uint32_t getMaxThreadsForVfe(const HardwareInfo &hwInfo);
    virtual uint32_t getMetricsLibraryGenId() const = 0;
    virtual uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const = 0;
    virtual bool tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) = 0;
    virtual uint32_t getBarriersCountFromHasBarriers(uint32_t hasBarriers) = 0;
    virtual uint32_t calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount,
                                                   uint32_t threadsPerEu) = 0;
    virtual uint32_t alignSlmSize(uint32_t slmSize) = 0;
    virtual uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) = 0;

    virtual bool isWaDisableRccRhwoOptimizationRequired() const = 0;
    virtual bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const = 0;
    virtual uint32_t getMinimalSIMDSize() = 0;
    virtual bool isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) const = 0;
    virtual bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const = 0;
    virtual uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const = 0;
    virtual uint32_t getBindlessSurfaceExtendedMessageDescriptorValue(uint32_t surfStateOffset) const = 0;
    virtual void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const = 0;
    virtual bool isBankOverrideRequired(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getGlobalTimeStampBits() const = 0;
    virtual int32_t getDefaultThreadArbitrationPolicy() const = 0;
    virtual bool useOnlyGlobalTimestamps() const = 0;
    virtual bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const = 0;
    virtual bool packedFormatsSupported() const = 0;
    virtual bool isAssignEngineRoundRobinSupported() const = 0;
    virtual bool isRcsAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                             const HardwareInfo &hwInfo, bool isEngineInstanced) const = 0;
    virtual size_t getMaxFillPaternSizeForCopyEngine() const = 0;
    virtual size_t getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSipWANeeded(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const = 0;
    virtual bool isKmdMigrationSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const = 0;
    virtual aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const = 0;
    virtual uint32_t getDefaultRevisionId(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getNumCacheRegions() const = 0;
    virtual bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const = 0;
    virtual uint32_t getPlanarYuvMaxHeight() const = 0;
    virtual size_t getPreemptionAllocationAlignment() const = 0;
    virtual std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const std::vector<uint32_t> &rootDeviceIndices, MemoryManager *memoryManager,
                                                                             size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                             DeviceBitfield deviceBitfield) const = 0;
    virtual size_t getTimestampPacketAllocatorAlignment() const = 0;
    virtual size_t getSingleTimestampPacketSize() const = 0;
    virtual void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const = 0;
    virtual void applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const = 0;
    virtual bool unTypedDataPortCacheFlushRequired() const = 0;
    virtual bool isEngineTypeRemappingToHwSpecificRequired() const = 0;

    static uint32_t getSubDevicesCount(const HardwareInfo *pHwInfo);
    static uint32_t getCopyEnginesCount(const HardwareInfo &hwInfo);

    virtual bool isSipKernelAsHexadecimalArrayPreferred() const = 0;
    virtual void setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize) const = 0;
    virtual void adjustPreemptionSurfaceSize(size_t &csrSize) const = 0;
    virtual size_t getSamplerStateSize() const = 0;

    virtual bool isScratchSpaceSurfaceStateAccessible() const = 0;
    virtual uint64_t getRenderSurfaceStateBaseAddress(void *renderSurfaceState) const = 0;
    virtual uint32_t getRenderSurfaceStatePitch(void *renderSurfaceState) const = 0;
    virtual size_t getMax3dImageWidthOrHeight() const = 0;
    virtual uint64_t getMaxMemAllocSize() const = 0;
    virtual bool isStatelesToStatefullWithOffsetSupported() const = 0;
    virtual void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) = 0;
    virtual bool disableL3CacheForDebug(const HardwareInfo &hwInfo) const = 0;
    virtual bool isRevisionSpecificBinaryBuiltinRequired() const = 0;
    virtual bool forceNonGpuCoherencyWA(bool requiresCoherency) const = 0;
    virtual bool platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool isLinuxCompletionFenceSupported() const = 0;
    virtual size_t getBatchBufferEndSize() const = 0;
    virtual const void *getBatchBufferEndReference() const = 0;
    virtual bool isPlatformFlushTaskEnabled(const NEO::HardwareInfo &hwInfo) const = 0;

  protected:
    HwHelper() = default;
};

template <typename GfxFamily>
class HwHelperHw : public HwHelper {
  public:
    static HwHelperHw<GfxFamily> &get() {
        static HwHelperHw<GfxFamily> hwHelper;
        return hwHelper;
    }

    uint32_t getBindingTableStateSurfaceStatePointer(const void *pBindingTable, uint32_t index) override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

        const BINDING_TABLE_STATE *bindingTableState = static_cast<const BINDING_TABLE_STATE *>(pBindingTable);
        return bindingTableState[index].getRawData(0);
    }

    size_t getBindingTableStateSize() const override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
        return sizeof(BINDING_TABLE_STATE);
    }

    uint32_t getBindingTableStateAlignement() const override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
        return BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE;
    }

    size_t getInterfaceDescriptorDataSize() const override {
        using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
        return sizeof(INTERFACE_DESCRIPTOR_DATA);
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

    uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const override;

    uint32_t getPitchAlignmentForImage(const HardwareInfo *hwInfo) const override;

    uint32_t getMaxNumSamplers() const override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo) override;

    bool isL3Configurable(const HardwareInfo &hwInfo) override;

    SipKernelType getSipKernelType(bool debuggingActive) const override;

    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override;

    bool hvAlign4Required() const override;

    bool isBufferSizeSuitableForCompression(const size_t size, const HardwareInfo &hwInfo) const override;

    bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) override;

    bool timestampPacketWriteSupported() const override;

    bool isTimestampWaitSupported() const override;

    bool isUpdateTaskCountFromWaitSupported() const override;

    bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const override;

    bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const override;

    void setRenderSurfaceStateForBuffer(const RootDeviceEnvironment &rootDeviceEnvironment,
                                        void *surfaceStateBuffer,
                                        size_t bufferSize,
                                        uint64_t gpuVa,
                                        size_t offset,
                                        uint32_t pitch,
                                        GraphicsAllocation *gfxAlloc,
                                        bool isReadOnly,
                                        uint32_t surfaceType,
                                        bool forceNonAuxMode,
                                        bool useL1Cache) override;

    MOCKABLE_VIRTUAL void setL1CachePolicy(bool useL1Cache, typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo);

    const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const override;

    EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override;

    const StackVec<size_t, 3> getDeviceSubGroupSizes() const override;

    const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const override;

    bool getEnableLocalMemory(const HardwareInfo &hwInfo) const override;

    std::string getExtensions() const override;

    uint32_t getMetricsLibraryGenId() const override;

    uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const override;

    bool tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) override;

    uint32_t getBarriersCountFromHasBarriers(uint32_t hasBarriers) override;

    uint32_t calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount, uint32_t threadsPerEu) override;

    uint32_t alignSlmSize(uint32_t slmSize) override;

    uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) override;

    static AuxTranslationMode getAuxTranslationMode(const HardwareInfo &hwInfo);

    bool isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) const override;

    bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const override;

    bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const override;

    static bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo);

    bool isWaDisableRccRhwoOptimizationRequired() const override;

    bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const override;

    uint32_t getMinimalSIMDSize() override;

    uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const override;

    uint32_t getGlobalTimeStampBits() const override;

    void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const override;

    bool isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const override;

    bool isBankOverrideRequired(const HardwareInfo &hwInfo) const override;

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

    bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override;

    bool isSipWANeeded(const HardwareInfo &hwInfo) const override;

    bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const override;

    aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const override;

    uint32_t getDefaultRevisionId(const HardwareInfo &hwInfo) const override;

    uint32_t getNumCacheRegions() const override;

    bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const override;

    uint32_t getPlanarYuvMaxHeight() const override;

    size_t getPreemptionAllocationAlignment() const override;

    std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const std::vector<uint32_t> &rootDeviceIndices, MemoryManager *memoryManager,
                                                                     size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                     DeviceBitfield deviceBitfield) const override;
    size_t getTimestampPacketAllocatorAlignment() const override;

    size_t getSingleTimestampPacketSize() const override;
    static size_t getSingleTimestampPacketSizeHw();

    void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const override;

    bool preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const override;

    void applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const override;

    bool unTypedDataPortCacheFlushRequired() const override;

    bool isAssignEngineRoundRobinSupported() const override;

    bool isEngineTypeRemappingToHwSpecificRequired() const override;

    bool isSipKernelAsHexadecimalArrayPreferred() const override;

    void setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize) const override;

    void adjustPreemptionSurfaceSize(size_t &csrSize) const override;

    bool isScratchSpaceSurfaceStateAccessible() const override;

    size_t getMax3dImageWidthOrHeight() const override;
    uint64_t getMaxMemAllocSize() const override;
    bool isStatelesToStatefullWithOffsetSupported() const override;
    void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) override;
    bool disableL3CacheForDebug(const HardwareInfo &hwInfo) const override;
    bool isRevisionSpecificBinaryBuiltinRequired() const override;
    bool forceNonGpuCoherencyWA(bool requiresCoherency) const override;
    bool platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const override;
    bool isLinuxCompletionFenceSupported() const override;
    size_t getBatchBufferEndSize() const override;
    const void *getBatchBufferEndReference() const override;
    bool isPlatformFlushTaskEnabled(const NEO::HardwareInfo &hwInfo) const override;

  protected:
    static const AuxTranslationMode defaultAuxTranslationMode;
    HwHelperHw() = default;
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
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;

    static void addPipeControlAndProgramPostSyncOperation(LinearStream &commandStream,
                                                          POST_SYNC_OPERATION operation,
                                                          uint64_t gpuAddress,
                                                          uint64_t immediateData,
                                                          const HardwareInfo &hwInfo,
                                                          PipeControlArgs &args);
    static void setPipeControlAndProgramPostSyncOperation(void *&commandsBuffer,
                                                          POST_SYNC_OPERATION operation,
                                                          uint64_t gpuAddress,
                                                          uint64_t immediateData,
                                                          const HardwareInfo &hwInfo,
                                                          PipeControlArgs &args);

    static void addPipeControlWithPostSync(LinearStream &commandStream,
                                           POST_SYNC_OPERATION operation,
                                           uint64_t gpuAddress,
                                           uint64_t immediateData,
                                           PipeControlArgs &args);
    static void setPipeControlWithPostSync(void *&commandsBuffer,
                                           POST_SYNC_OPERATION operation,
                                           uint64_t gpuAddress,
                                           uint64_t immediateData,
                                           PipeControlArgs &args);

    static void setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo);
    static void setPipeControlWAFlags(PIPE_CONTROL &pipeControl);

    static void addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo);
    static void setPipeControlWA(void *&commandsBuffer, uint64_t gpuAddress, const HardwareInfo &hwInfo);

    static void addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo);
    static void setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo);

    static void addPipeControl(LinearStream &commandStream, PipeControlArgs &args);
    static void setPipeControl(PIPE_CONTROL &pipeControl, PipeControlArgs &args);

    static void addPipeControlWithCSStallOnly(LinearStream &commandStream);

    static bool getDcFlushEnable(bool isFlushPreferred, const HardwareInfo &hwInfo);

    static void addFullCacheFlush(LinearStream &commandStream, const HardwareInfo &hwInfo);
    static void setCacheFlushExtraProperties(PipeControlArgs &args);

    static size_t getSizeForPipeControlWithPostSyncOperation(const HardwareInfo &hwInfo);
    static size_t getSizeForPipeControlWA(const HardwareInfo &hwInfo);
    static size_t getSizeForSinglePipeControl();
    static size_t getSizeForSingleAdditionalSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForAdditonalSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForFullCacheFlush();

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);
    static bool isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo);

  protected:
    static void setPipeControlExtraProperties(PIPE_CONTROL &pipeControl, PipeControlArgs &args);
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

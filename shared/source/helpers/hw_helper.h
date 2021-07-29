/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/options.h"
#include "shared/source/utilities/stackvec.h"

#include "aub_mem_dump.h"
#include "engine_group_types.h"
#include "hw_cmds.h"
#include "third_party/aub_stream/headers/aubstream.h"

#include <cstdint>
#include <string>
#include <type_traits>

namespace NEO {
class GmmHelper;
class GraphicsAllocation;
class TagAllocatorBase;
class Gmm;
struct AllocationData;
struct AllocationProperties;
struct EngineControl;
struct HardwareCapabilities;
struct RootDeviceEnvironment;
struct PipeControlArgs;

enum class LocalMemoryAccessMode {
    Default = 0,
    CpuAccessAllowed = 1,
    CpuAccessDisallowed = 3
};

class HwHelper {
  public:
    using EngineInstancesContainer = StackVec<EngineTypeUsage, 32>;
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
    virtual void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) = 0;
    virtual void adjustDefaultEngineType(HardwareInfo *pHwInfo) = 0;
    virtual void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) = 0;
    virtual bool isL3Configurable(const HardwareInfo &hwInfo) = 0;
    virtual SipKernelType getSipKernelType(bool debuggingActive) const = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const = 0;
    virtual bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const = 0;
    virtual bool hvAlign4Required() const = 0;
    virtual bool preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const = 0;
    virtual bool isBufferSizeSuitableForRenderCompression(const size_t size, const HardwareInfo &hwInfo) const = 0;
    virtual bool obtainBlitterPreference(const HardwareInfo &hwInfo) const = 0;
    virtual bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) = 0;
    virtual bool allowRenderCompression(const HardwareInfo &hwInfo) const = 0;
    virtual bool allowStatelessCompression(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const = 0;
    virtual LocalMemoryAccessMode getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;
    static bool renderCompressedBuffersSupported(const HardwareInfo &hwInfo);
    static bool renderCompressedImagesSupported(const HardwareInfo &hwInfo);
    static bool cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo);
    virtual bool timestampPacketWriteSupported() const = 0;
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
    virtual EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, const HardwareInfo &hwInfo) const = 0;
    virtual const StackVec<size_t, 3> getDeviceSubGroupSizes() const = 0;
    virtual const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const = 0;
    virtual bool getEnableLocalMemory(const HardwareInfo &hwInfo) const = 0;
    virtual std::string getExtensions() const = 0;
    virtual std::string getDeviceMemoryName() const = 0;
    static uint32_t getMaxThreadsForVfe(const HardwareInfo &hwInfo);
    virtual uint32_t getMetricsLibraryGenId() const = 0;
    virtual uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const = 0;
    virtual bool tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) = 0;
    virtual uint32_t getBarriersCountFromHasBarriers(uint32_t hasBarriers) = 0;
    virtual uint32_t calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount,
                                                   uint32_t threadsPerEu) = 0;
    virtual uint32_t alignSlmSize(uint32_t slmSize) = 0;
    virtual uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) = 0;

    virtual bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) = 0;
    virtual bool isWaDisableRccRhwoOptimizationRequired() const = 0;
    virtual bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const = 0;
    virtual uint32_t getMinimalSIMDSize() = 0;
    virtual uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const = 0;
    virtual bool isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) const = 0;
    virtual bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const = 0;
    virtual uint32_t getBindlessSurfaceExtendedMessageDescriptorValue(uint32_t surfStateOffset) const = 0;
    virtual void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const = 0;
    virtual bool isBankOverrideRequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSpecialWorkgroupSizeRequired(const HardwareInfo &hwInfo, bool isSimulation) const = 0;
    virtual uint32_t getGlobalTimeStampBits() const = 0;
    virtual uint32_t getDefaultThreadArbitrationPolicy() const = 0;
    virtual bool heapInLocalMem(const HardwareInfo &hwInfo) const = 0;
    virtual bool useOnlyGlobalTimestamps() const = 0;
    virtual bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const = 0;
    virtual bool packedFormatsSupported() const = 0;
    virtual bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const PRODUCT_FAMILY productFamily) const = 0;
    virtual size_t getMaxFillPaternSizeForCopyEngine() const = 0;
    virtual bool isCopyOnlyEngineType(EngineGroupType type) const = 0;
    virtual void adjustAddressWidthForCanonize(uint32_t &addressWidth) const = 0;
    virtual bool isSipWANeeded(const HardwareInfo &hwInfo) const = 0;
    virtual bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const = 0;
    virtual bool isKmdMigrationSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isNewResidencyModelSupported() const = 0;
    virtual bool isDirectSubmissionSupported(const HardwareInfo &hwInfo) const = 0;
    virtual aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const = 0;
    virtual uint32_t getDefaultRevisionId(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getNumCacheRegions() const = 0;
    virtual bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const = 0;
    virtual uint32_t getPlanarYuvMaxHeight() const = 0;
    virtual bool isBlitterForImagesSupported(const HardwareInfo &hwInfo) const = 0;
    virtual size_t getPreemptionAllocationAlignment() const = 0;
    virtual std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const std::vector<uint32_t> &rootDeviceIndices, MemoryManager *memoryManager,
                                                                             size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                             DeviceBitfield deviceBitfield) const = 0;
    virtual size_t getTimestampPacketAllocatorAlignment() const = 0;
    virtual size_t getSingleTimestampPacketSize() const = 0;
    virtual void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const = 0;
    virtual void applyRenderCompressionFlag(Gmm &gmm, uint32_t isRenderCompressed) const = 0;
    virtual bool additionalPipeControlArgsRequired() const = 0;

    static uint32_t getSubDevicesCount(const HardwareInfo *pHwInfo);
    static uint32_t getGpgpuEnginesCount(const HardwareInfo &hwInfo);
    static uint32_t getCopyEnginesCount(const HardwareInfo &hwInfo);

  protected:
    virtual LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const = 0;

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

    uint32_t getBindlessSurfaceExtendedMessageDescriptorValue(uint32_t surfStateOffset) const override {
        using DataPortBindlessSurfaceExtendedMessageDescriptor = typename GfxFamily::DataPortBindlessSurfaceExtendedMessageDescriptor;
        DataPortBindlessSurfaceExtendedMessageDescriptor messageExtDescriptor = {};
        messageExtDescriptor.setBindlessSurfaceOffset(surfStateOffset);
        return messageExtDescriptor.getBindlessSurfaceOffsetToPatch();
    }

    const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const override;

    size_t getMaxBarrierRegisterPerSlice() const override;

    size_t getPaddingForISAAllocation() const override;

    uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const override;

    uint32_t getPitchAlignmentForImage(const HardwareInfo *hwInfo) const override;

    uint32_t getMaxNumSamplers() const override;

    void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo) override;

    void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) override;

    bool isL3Configurable(const HardwareInfo &hwInfo) override;

    SipKernelType getSipKernelType(bool debuggingActive) const override;

    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override;

    bool heapInLocalMem(const HardwareInfo &hwInfo) const override;

    bool hvAlign4Required() const override;

    bool isBufferSizeSuitableForRenderCompression(const size_t size, const HardwareInfo &hwInfo) const override;

    bool obtainBlitterPreference(const HardwareInfo &hwInfo) const override;

    bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) override;

    bool timestampPacketWriteSupported() const override;

    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override;

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

    EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, const HardwareInfo &hwInfo) const override;

    const StackVec<size_t, 3> getDeviceSubGroupSizes() const override;

    const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const override;

    bool getEnableLocalMemory(const HardwareInfo &hwInfo) const override;

    std::string getExtensions() const override;

    std::string getDeviceMemoryName() const override;

    uint32_t getMetricsLibraryGenId() const override;

    uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const override;

    bool tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) override;

    uint32_t getBarriersCountFromHasBarriers(uint32_t hasBarriers) override;

    uint32_t calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount, uint32_t threadsPerEu) override;

    uint32_t alignSlmSize(uint32_t slmSize) override;

    uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) override;

    static AuxTranslationMode getAuxTranslationMode(const HardwareInfo &hwInfo);

    uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const override;

    uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;

    uint32_t getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;

    bool isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) const override;

    bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const override;

    bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) const override;

    bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo) const override;

    static bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo);

    bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) override;

    bool isWaDisableRccRhwoOptimizationRequired() const override;

    bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const override;

    uint32_t getMinimalSIMDSize() override;

    uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const override;

    bool isSpecialWorkgroupSizeRequired(const HardwareInfo &hwInfo, bool isSimulation) const override;

    uint32_t getGlobalTimeStampBits() const override;

    void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const override;

    bool allowRenderCompression(const HardwareInfo &hwInfo) const override;

    bool allowStatelessCompression(const HardwareInfo &hwInfo) const override;

    bool isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const override;

    LocalMemoryAccessMode getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const override;

    bool isBankOverrideRequired(const HardwareInfo &hwInfo) const override;

    uint32_t getDefaultThreadArbitrationPolicy() const override;

    bool useOnlyGlobalTimestamps() const override;

    bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const override;

    bool packedFormatsSupported() const override;

    bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const PRODUCT_FAMILY productFamily) const override;

    size_t getMaxFillPaternSizeForCopyEngine() const override;

    bool isKmdMigrationSupported(const HardwareInfo &hwInfo) const override;

    bool isNewResidencyModelSupported() const override;

    bool isDirectSubmissionSupported(const HardwareInfo &hwInfo) const override;

    bool isCopyOnlyEngineType(EngineGroupType type) const override;

    void adjustAddressWidthForCanonize(uint32_t &addressWidth) const override;

    bool isSipWANeeded(const HardwareInfo &hwInfo) const override;

    bool isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const override;

    bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const override;

    aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const override;

    uint32_t getDefaultRevisionId(const HardwareInfo &hwInfo) const override;

    uint32_t getNumCacheRegions() const override;

    bool isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const override;

    uint32_t getPlanarYuvMaxHeight() const override;

    bool isBlitterForImagesSupported(const HardwareInfo &hwInfo) const override;

    size_t getPreemptionAllocationAlignment() const override;

    std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const std::vector<uint32_t> &rootDeviceIndices, MemoryManager *memoryManager,
                                                                     size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                     DeviceBitfield deviceBitfield) const override;
    size_t getTimestampPacketAllocatorAlignment() const override;

    size_t getSingleTimestampPacketSize() const override;

    void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const override;

    bool preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const override;

    void applyRenderCompressionFlag(Gmm &gmm, uint32_t isRenderCompressed) const override;

    bool additionalPipeControlArgsRequired() const override;

  protected:
    LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const override;

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
    static void addPipeControlWithPostSync(LinearStream &commandStream,
                                           POST_SYNC_OPERATION operation,
                                           uint64_t gpuAddress,
                                           uint64_t immediateData,
                                           PipeControlArgs &args);
    static void setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo);

    static void addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo);
    static void addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo);

    static void addPipeControl(LinearStream &commandStream, PipeControlArgs &args);
    static void addPipeControlWithCSStallOnly(LinearStream &commandStream);

    static bool isDcFlushAllowed();

    static void addFullCacheFlush(LinearStream &commandStream);
    static void setCacheFlushExtraProperties(PipeControlArgs &args);

    static size_t getSizeForPipeControlWithPostSyncOperation(const HardwareInfo &hwInfo);
    static size_t getSizeForSinglePipeControl();
    static size_t getSizeForSingleSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForAdditonalSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForFullCacheFlush();

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);
    static bool isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo);

  protected:
    static void setPipeControl(PIPE_CONTROL &pipeControl, PipeControlArgs &args);
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

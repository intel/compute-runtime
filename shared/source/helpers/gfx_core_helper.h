/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/options.h"
#include "shared/source/utilities/stackvec.h"

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
enum class DebuggingMode : uint32_t;
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
struct KernelDescriptor;
class ProductHelper;
class GfxCoreHelper;
class AILConfiguration;

using EngineInstancesContainer = StackVec<EngineTypeUsage, 32>;
using GfxCoreHelperCreateFunctionType = std::unique_ptr<GfxCoreHelper> (*)();

extern const char *deviceHierarchyComposite;
extern const char *deviceHierarchyFlat;
extern const char *deviceHierarchyCombined;
extern const char *deviceHierarchyUnk;

class GfxCoreHelper {
  public:
    static std::unique_ptr<GfxCoreHelper> create(const GFXCORE_FAMILY gfxCoreFamily);
    virtual size_t getMaxBarrierRegisterPerSlice() const = 0;
    virtual size_t getPaddingForISAAllocation() const = 0;
    virtual size_t getKernelIsaPointerAlignment() const = 0;
    virtual uint32_t getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual uint32_t getPitchAlignmentForImage(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual void adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) = 0;
    virtual SipKernelType getSipKernelType(bool debuggingActive) const = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const = 0;
    virtual bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const = 0;
    virtual bool hvAlign4Required() const = 0;
    virtual bool isBufferSizeSuitableForCompression(const size_t size) const = 0;
    virtual bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) const = 0;
    static bool compressedBuffersSupported(const HardwareInfo &hwInfo);
    static bool compressedImagesSupported(const HardwareInfo &hwInfo);
    static bool cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo);
    static uint32_t getHighestEnabledSlice(const HardwareInfo &hwInfo);
    static uint32_t getHighestEnabledDualSubSlice(const HardwareInfo &hwInfo);
    virtual bool timestampPacketWriteSupported() const = 0;
    virtual bool isTimestampWaitSupportedForQueues() const = 0;
    virtual bool isUpdateTaskCountFromWaitSupported() const = 0;
    virtual bool makeResidentBeforeLockNeeded(bool precondition) const = 0;
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
    virtual const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual uint32_t getInternalCopyEngineIndex(const HardwareInfo &hwInfo) const = 0;
    virtual EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const = 0;
    virtual const StackVec<size_t, 3> getDeviceSubGroupSizes() const = 0;
    virtual bool getEnableLocalMemory(const HardwareInfo &hwInfo) const = 0;
    static uint32_t getMaxThreadsForVfe(const HardwareInfo &hwInfo);
    virtual uint32_t getMetricsLibraryGenId() const = 0;
    virtual uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const = 0;
    virtual uint8_t getBarriersCountFromHasBarriers(uint8_t hasBarriers) const = 0;
    virtual uint32_t calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const = 0;
    virtual uint32_t calculateMaxWorkGroupSize(const KernelDescriptor &kernelDescriptor, uint32_t defaultMaxGroupSize) const = 0;
    virtual bool largeGrfModeSupported() const = 0;
    virtual uint32_t alignSlmSize(uint32_t slmSize) const = 0;
    virtual uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const = 0;

    virtual bool isWaDisableRccRhwoOptimizationRequired() const = 0;
    virtual bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const = 0;
    virtual uint32_t getMinimalSIMDSize() const = 0;
    virtual uint32_t getMinimalGrfSize() const = 0;
    virtual bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const = 0;
    virtual bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const = 0;
    virtual uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double resolution) const = 0;
    virtual uint32_t getBindlessSurfaceExtendedMessageDescriptorValue(uint32_t surfStateOffset) const = 0;
    virtual void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual bool isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const = 0;
    virtual uint32_t getGlobalTimeStampBits() const = 0;
    virtual int32_t getDefaultThreadArbitrationPolicy() const = 0;
    virtual bool useOnlyGlobalTimestamps() const = 0;
    virtual bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const = 0;
    virtual bool packedFormatsSupported() const = 0;
    virtual bool isRcsAvailable(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual uint32_t adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                             const RootDeviceEnvironment &rootDeviceEnvironment, bool isEngineInstanced) const = 0;
    virtual uint32_t adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, bool isHwLocalGeneration, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual size_t getMaxFillPaternSizeForCopyEngine() const = 0;
    virtual size_t getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const = 0;
    virtual bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const = 0;
    virtual aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const = 0;
    virtual bool isSubDeviceEngineSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const = 0;
    virtual uint32_t getPlanarYuvMaxHeight() const = 0;
    virtual size_t getPreemptionAllocationAlignment() const = 0;
    virtual std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager,
                                                                             size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                             DeviceBitfield deviceBitfield) const = 0;
    virtual size_t getTimestampPacketAllocatorAlignment() const = 0;
    virtual size_t getSingleTimestampPacketSize() const = 0;
    virtual void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const = 0;
    virtual bool isRunaloneModeRequired(DebuggingMode debuggingMode) const = 0;
    virtual void applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const = 0;
    virtual bool unTypedDataPortCacheFlushRequired() const = 0;
    virtual bool isEngineTypeRemappingToHwSpecificRequired() const = 0;

    static uint32_t getSubDevicesCount(const HardwareInfo *pHwInfo);

    virtual bool isSipKernelAsHexadecimalArrayPreferred() const = 0;
    virtual void setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual void adjustPreemptionSurfaceSize(size_t &csrSize, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual size_t getSamplerStateSize() const = 0;
    virtual bool preferInternalBcsEngine() const = 0;
    virtual bool isScratchSpaceSurfaceStateAccessible() const = 0;
    virtual uint32_t getMaxScratchSize(const NEO::ProductHelper &productHelper) const = 0;
    virtual uint64_t getRenderSurfaceStateBaseAddress(void *renderSurfaceState) const = 0;
    virtual uint32_t getRenderSurfaceStatePitch(void *renderSurfaceState, const ProductHelper &productHelper) const = 0;
    virtual size_t getMax3dImageWidthOrHeight() const = 0;
    virtual uint64_t getMaxMemAllocSize() const = 0;
    virtual bool isStatelessToStatefulWithOffsetSupported() const = 0;
    virtual void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const = 0;
    virtual bool platformSupportsImplicitScaling(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual size_t getBatchBufferEndSize() const = 0;
    virtual const void *getBatchBufferEndReference() const = 0;
    virtual size_t getBatchBufferStartSize() const = 0;
    virtual void encodeBatchBufferStart(void *cmdBuffer, uint64_t address, bool secondLevel, bool indirect, bool predicate) const = 0;
    virtual bool isPlatformFlushTaskEnabled(const NEO::ProductHelper &productHelper) const = 0;
    virtual uint32_t getMinimalScratchSpaceSize() const = 0;
    virtual bool copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const = 0;
    virtual uint32_t getAmountOfAllocationsToFill() const = 0;
    virtual bool isChipsetUniqueUUIDSupported() const = 0;
    virtual bool isTimestampShiftRequired() const = 0;
    virtual bool isRelaxedOrderingSupported() const = 0;
    virtual uint32_t calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, bool isHwLocalIdGeneration, const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual uint32_t overrideMaxWorkGroupSize(uint32_t maxWG) const = 0;
    virtual char const *getDefaultDeviceHierarchy() const = 0;
    static bool isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo, const ProductHelper &productHelper);

    virtual bool areSecondaryContextsSupported() const = 0;
    virtual uint32_t getContextGroupContextsCount() const = 0;
    virtual uint32_t getContextGroupHpContextsCount(EngineGroupType type, bool hpEngineAvailable) const = 0;
    virtual aub_stream::EngineType getDefaultHpCopyEngine(const HardwareInfo &hwInfo) const = 0;

    virtual bool is48ResourceNeededForCmdBuffer() const = 0;
    virtual uint32_t getKernelPrivateMemSize(const KernelDescriptor &kernelDescriptor) const = 0;

    virtual bool singleTileExecImplicitScalingRequired(bool cooperativeKernel) const = 0;
    virtual bool duplicatedInOrderCounterStorageEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual bool inOrderAtomicSignallingEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual bool isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                     const size_t *lws,
                                                     std::array<uint8_t, 3> &walkOrder,
                                                     bool requireInputWalkOrder,
                                                     uint32_t &requiredWalkOrder,
                                                     uint32_t simd) const = 0;
    virtual uint32_t getMaxPtssIndex(const ProductHelper &productHelper) const = 0;
    virtual uint32_t getDefaultSshSize(const ProductHelper &productHelper) const = 0;

    virtual bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool createMemoryInfoSupported() const = 0;

    virtual ~GfxCoreHelper() = default;

  protected:
    GfxCoreHelper() = default;
};

template <typename GfxFamily>
class GfxCoreHelperHw : public GfxCoreHelper {
  public:
    static std::unique_ptr<GfxCoreHelper> create() {
        auto gfxCoreHelper = std::unique_ptr<GfxCoreHelper>(new GfxCoreHelperHw<GfxFamily>());
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

    uint32_t getRenderSurfaceStatePitch(void *renderSurfaceState, const ProductHelper &productHelper) const override;

    const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const override;

    size_t getMaxBarrierRegisterPerSlice() const override;

    size_t getPaddingForISAAllocation() const override;

    size_t getKernelIsaPointerAlignment() const override {
        return static_cast<size_t>(GfxFamily::cmdInitInterfaceDescriptorData.KERNELSTARTPOINTER_ALIGN_SIZE);
    }

    uint32_t getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    uint32_t getPitchAlignmentForImage(const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) override;

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

    bool makeResidentBeforeLockNeeded(bool precondition) const override;

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

    const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    uint32_t getInternalCopyEngineIndex(const HardwareInfo &hwInfo) const override;

    EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override;

    const StackVec<size_t, 3> getDeviceSubGroupSizes() const override;

    bool getEnableLocalMemory(const HardwareInfo &hwInfo) const override;

    uint32_t getMetricsLibraryGenId() const override;

    uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const override;

    uint8_t getBarriersCountFromHasBarriers(uint8_t hasBarriers) const override;

    uint32_t calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const override;

    uint32_t calculateMaxWorkGroupSize(const KernelDescriptor &kernelDescriptor, uint32_t defaultMaxGroupSize) const override;
    bool largeGrfModeSupported() const override;

    uint32_t alignSlmSize(uint32_t slmSize) const override;

    uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const override;

    static AuxTranslationMode getAuxTranslationMode(const HardwareInfo &hwInfo);

    bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override;

    bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const override;

    static bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo);

    bool isWaDisableRccRhwoOptimizationRequired() const override;

    bool isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const override;

    uint32_t getMinimalSIMDSize() const override;

    uint32_t getMinimalGrfSize() const override;

    uint64_t getGpuTimeStampInNS(uint64_t timeStamp, double resolution) const override;

    uint32_t getGlobalTimeStampBits() const override;

    void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    bool isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override;

    int32_t getDefaultThreadArbitrationPolicy() const override;

    bool useOnlyGlobalTimestamps() const override;

    bool useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const override;

    bool packedFormatsSupported() const override;

    bool isRcsAvailable(const HardwareInfo &hwInfo) const override;

    bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    uint32_t adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                     const RootDeviceEnvironment &rootDeviceEnvironment, bool isEngineInstanced) const override;

    uint32_t adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, bool isHwLocalGeneration, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const override;
    size_t getMaxFillPaternSizeForCopyEngine() const override;

    size_t getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const override;

    bool isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const override;

    aub_stream::MMIOList getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const override;

    bool isSubDeviceEngineSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const override;

    uint32_t getPlanarYuvMaxHeight() const override;

    size_t getPreemptionAllocationAlignment() const override;

    std::unique_ptr<TagAllocatorBase> createTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager,
                                                                     size_t initialTagCount, CommandStreamReceiverType csrType,
                                                                     DeviceBitfield deviceBitfield) const override;
    size_t getTimestampPacketAllocatorAlignment() const override;

    size_t getSingleTimestampPacketSize() const override;
    static size_t getSingleTimestampPacketSizeHw();

    void applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const override;

    bool isRunaloneModeRequired(DebuggingMode debuggingMode) const override;

    void applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const override;

    bool unTypedDataPortCacheFlushRequired() const override;
    bool isEngineTypeRemappingToHwSpecificRequired() const override;

    bool isSipKernelAsHexadecimalArrayPreferred() const override;

    void setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize, const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    void adjustPreemptionSurfaceSize(size_t &csrSize, const RootDeviceEnvironment &rootDeviceEnvironment) const override;
    bool isScratchSpaceSurfaceStateAccessible() const override;
    uint32_t getMaxScratchSize(const NEO::ProductHelper &productHelper) const override;
    bool preferInternalBcsEngine() const override;
    size_t getMax3dImageWidthOrHeight() const override;
    uint64_t getMaxMemAllocSize() const override;
    bool isStatelessToStatefulWithOffsetSupported() const override;
    void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override;
    bool platformSupportsImplicitScaling(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const override;
    size_t getBatchBufferEndSize() const override;
    const void *getBatchBufferEndReference() const override;
    size_t getBatchBufferStartSize() const override;
    void encodeBatchBufferStart(void *cmdBuffer, uint64_t address, bool secondLevel, bool indirect, bool predicate) const override;
    bool isPlatformFlushTaskEnabled(const NEO::ProductHelper &productHelper) const override;
    uint32_t getMinimalScratchSpaceSize() const override;
    bool copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override;
    uint32_t getAmountOfAllocationsToFill() const override;
    bool isChipsetUniqueUUIDSupported() const override;
    bool isTimestampShiftRequired() const override;
    bool isRelaxedOrderingSupported() const override;
    uint32_t calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, bool isHwLocalIdGeneration, const RootDeviceEnvironment &rootDeviceEnvironment) const override;
    uint32_t overrideMaxWorkGroupSize(uint32_t maxWG) const override;
    char const *getDefaultDeviceHierarchy() const override;

    bool areSecondaryContextsSupported() const override;
    uint32_t getContextGroupContextsCount() const override;
    uint32_t getContextGroupHpContextsCount(EngineGroupType type, bool hpEngineAvailable) const override;
    aub_stream::EngineType getDefaultHpCopyEngine(const HardwareInfo &hwInfo) const override;

    bool is48ResourceNeededForCmdBuffer() const override;

    uint32_t getKernelPrivateMemSize(const KernelDescriptor &kernelDescriptor) const override;

    bool singleTileExecImplicitScalingRequired(bool cooperativeKernel) const override;
    bool duplicatedInOrderCounterStorageEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const override;
    bool inOrderAtomicSignallingEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const override;

    bool isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                             const size_t *lws,
                                             std::array<uint8_t, 3> &walkOrder,
                                             bool requireInputWalkOrder,
                                             uint32_t &requiredWalkOrder,
                                             uint32_t simd) const override;
    uint32_t getMaxPtssIndex(const ProductHelper &productHelper) const override;
    uint32_t getDefaultSshSize(const ProductHelper &productHelper) const override;

    bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool createMemoryInfoSupported() const override;

    ~GfxCoreHelperHw() override = default;

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

    static void *program(LinearStream *cmdStream, uint32_t address, uint32_t value, bool remap, bool isBcs);
    static void *program(MI_LOAD_REGISTER_IMM *lriCmd, uint32_t address, uint32_t value, bool remap, bool isBcs);
};

template <typename GfxFamily>
struct MemorySynchronizationCommands {
    static void addSingleBarrier(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args);
    static void setSingleBarrier(void *commandsBuffer, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args);
    static void addSingleBarrier(LinearStream &commandStream, PipeControlArgs &args);
    static void setSingleBarrier(void *commandsBuffer, PipeControlArgs &args);

    static void addBarrierWithPostSyncOperation(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, const RootDeviceEnvironment &rootDeviceEnvironment, PipeControlArgs &args);
    static void setBarrierWithPostSyncOperation(void *&commandsBuffer, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, const RootDeviceEnvironment &rootDeviceEnvironment, PipeControlArgs &args);

    static void setPostSyncExtraProperties(PipeControlArgs &args);

    static void addBarrierWa(LinearStream &commandStream, uint64_t gpuAddress, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void setBarrierWa(void *&commandsBuffer, uint64_t gpuAddress, const RootDeviceEnvironment &rootDeviceEnvironment);

    static void setBarrierWaFlags(void *barrierCmd);

    static void addAdditionalSynchronizationForDirectSubmission(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment);

    static bool getDcFlushEnable(bool isFlushPreferred, const RootDeviceEnvironment &rootDeviceEnvironment);

    static void addFullCacheFlush(LinearStream &commandStream, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void setCacheFlushExtraProperties(PipeControlArgs &args);
    static void addStateCacheFlush(LinearStream &commandStream, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void addInstructionCacheFlush(LinearStream &commandStream);

    static size_t getSizeForBarrierWithPostSyncOperation(const RootDeviceEnvironment &rootDeviceEnvironment, bool tlbInvalidationRequired);
    static size_t getSizeForBarrierWa(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeForSingleBarrier(bool tlbInvalidationRequired);
    static size_t getSizeForSingleAdditionalSynchronizationForDirectSubmission(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeForSingleAdditionalSynchronization(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeForAdditonalSynchronization(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeForInstructionCacheFlush();
    static size_t getSizeForFullCacheFlush();

    static bool isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment);
    static bool isBarrierPriorToPipelineSelectWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment);
    static void setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args);

    static void encodeAdditionalTimestampOffsets(LinearStream &commandStream, uint64_t contextAddress, uint64_t globalAddress);
};

} // namespace NEO

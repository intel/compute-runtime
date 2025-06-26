/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/vec.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"

namespace NEO {
enum class ImageType;
enum class MemoryPool;
enum class TransferDirection;
struct BlitProperties;
struct EncodeDispatchKernelArgs;
struct KernelDescriptor;

} // namespace NEO

namespace L0 {
enum class Builtin : uint32_t;

struct Event;
struct EventPool;

#pragma pack(1)
struct EventData {
    uint64_t address;
    uint64_t packetsInUse;
    uint64_t timestampSizeInDw;
};
#pragma pack()

static_assert(sizeof(EventData) == (3 * sizeof(uint64_t)),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

struct AlignedAllocationData {
    uintptr_t alignedAllocationPtr = 0u;
    size_t offset = 0u;
    NEO::GraphicsAllocation *alloc = nullptr;
    bool needsFlush = false;
};

struct CmdListFillKernelArguments {
    size_t mainOffset = 0;
    size_t mainGroupSize = 0;
    size_t groups = 0;
    size_t rightOffset = 0;
    size_t patternOffsetRemainder = 0;
    uint32_t leftRemainingBytes = 0;
    uint32_t rightRemainingBytes = 0;
    uint32_t patternSizeInEls = 0;
};

struct CmdListEventOperation {
    size_t operationOffset = 0;
    size_t completionFieldOffset = 0;
    uint32_t operationCount = 0;
    bool workPartitionOperation = false;
    bool isTimestmapEvent = false;
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamily : public CommandListImp {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    using CommandListImp::skipInOrderNonWalkerSignalingAllowed;

    using CommandListImp::CommandListImp;
    ze_result_t initialize(Device *device, NEO::EngineGroupType engineGroupType, ze_command_list_flags_t flags) override;
    void programL3(bool isSLMused);
    ~CommandListCoreFamily() override;

    ze_result_t close() override;
    ze_result_t appendEventReset(ze_event_handle_t hEvent) override;
    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;
    ze_result_t appendMemoryRangesBarrier(uint32_t numRanges,
                                          const size_t *pRangeSizes,
                                          const void **pRanges,
                                          ze_event_handle_t hSignalEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage, const void *srcptr,
                                          const ze_image_region_t *pDstRegion,
                                          ze_event_handle_t hEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendImageCopyToMemory(void *dstptr, ze_image_handle_t hSrcImage,
                                        const ze_image_region_t *pSrcRegion, ze_event_handle_t hEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendImageCopyFromMemoryExt(ze_image_handle_t hDstImage, const void *srcptr,
                                             const ze_image_region_t *pDstRegion,
                                             uint32_t srcRowPitch, uint32_t srcSlicePitch,
                                             ze_event_handle_t hEvent,
                                             uint32_t numWaitEvents,
                                             ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendImageCopyToMemoryExt(void *dstptr, ze_image_handle_t hSrcImage,
                                           const ze_image_region_t *pSrcRegion,
                                           uint32_t destRowPitch, uint32_t destSlicePitch,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendImageCopyRegion(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                      const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion,
                                      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendImageCopy(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                   const ze_group_count_t &threadGroupDimensions,
                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   CmdListKernelLaunchParams &launchParams) override;
    ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                           const ze_group_count_t &pDispatchArgumentsBuffer,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;
    ze_result_t appendLaunchMultipleKernelsIndirect(uint32_t numKernels,
                                                    const ze_kernel_handle_t *kernelHandles,
                                                    const uint32_t *pNumLaunchArguments,
                                                    const ze_group_count_t *pLaunchArgumentsBuffer,
                                                    ze_event_handle_t hEvent,
                                                    uint32_t numWaitEvents,
                                                    ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;
    ze_result_t appendLaunchKernelWithArguments(ze_kernel_handle_t hKernel,
                                                const ze_group_count_t groupCounts,
                                                const ze_group_size_t groupSizes,

                                                void **pArguments,
                                                void *pNext,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendMemAdvise(ze_device_handle_t hDevice,
                                const void *ptr, size_t size,
                                ze_memory_advice_t advice) override;
    ze_result_t executeMemAdvise(ze_device_handle_t hDevice,
                                 const void *ptr, size_t size,
                                 ze_memory_advice_t advice) override;
    ze_result_t appendMemoryCopy(void *dstptr, const void *srcptr, size_t size,
                                 ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                    NEO::GraphicsAllocation *srcAllocation,
                                    size_t size,
                                    bool flushHost) override;
    ze_result_t appendMemoryCopyRegion(void *dstPtr,
                                       const ze_copy_region_t *dstRegion,
                                       uint32_t dstPitch,
                                       uint32_t dstSlicePitch,
                                       const void *srcPtr,
                                       const ze_copy_region_t *srcRegion,
                                       uint32_t srcPitch,
                                       uint32_t srcSlicePitch,
                                       ze_event_handle_t hSignalEvent,
                                       uint32_t numWaitEvents,
                                       ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendMemoryPrefetch(const void *ptr, size_t count) override;
    ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                 size_t patternSize, size_t size,
                                 ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;

    ze_result_t appendMILoadRegImm(uint32_t reg, uint32_t value, bool isBcs) override;
    ze_result_t appendMILoadRegReg(uint32_t reg1, uint32_t reg2) override;
    ze_result_t appendMILoadRegMem(uint32_t reg1, uint64_t address) override;
    ze_result_t appendMIStoreRegMem(uint32_t reg1, uint64_t address) override;
    ze_result_t appendMIMath(void *aluArray, size_t aluCount) override;
    ze_result_t appendMIBBStart(uint64_t address, size_t predication, bool secondLevel) override;
    ze_result_t appendMIBBEnd() override;
    ze_result_t appendMINoop() override;
    ze_result_t appendPipeControl(void *dstPtr, uint64_t value) override;
    ze_result_t appendSoftwareTag(const char *data) override;
    ze_result_t appendWaitOnMemory(void *desc, void *ptr, uint64_t data, ze_event_handle_t signalEventHandle, bool useQwordData) override;
    ze_result_t appendWriteToMemory(void *desc, void *ptr,
                                    uint64_t data) override;

    ze_result_t appendWaitExternalSemaphores(uint32_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                             const ze_external_semaphore_wait_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendSignalExternalSemaphores(size_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                               const ze_external_semaphore_signal_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendQueryKernelTimestamps(uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr,
                                            const size_t *pOffsets, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    ze_result_t hostSynchronize(uint64_t timeout) override;

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent, bool relaxedOrderingDispatch) override;
    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                   bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) override;
    void appendWaitOnInOrderDependency(std::shared_ptr<NEO::InOrderExecInfo> &inOrderExecInfo, CommandToPatchContainer *outListCommands,
                                       uint64_t waitValue, uint32_t offset, bool relaxedOrderingAllowed, bool implicitDependency,
                                       bool skipAddingWaitEventsToResidency, bool noopDispatch, bool dualStreamCopyOffloadOperation);
    MOCKABLE_VIRTUAL void appendSignalInOrderDependencyCounter(Event *signalEvent, bool copyOffloadOperation, bool stall, bool textureFlushRequired);
    void handleInOrderDependencyCounter(Event *signalEvent, bool nonWalkerInOrderCmdsChaining, bool copyOffloadOperation);
    void handleInOrderCounterOverflow(bool copyOffloadOperation);

    ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
                                            size_t size, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override;
    void appendMultiPartitionPrologue(uint32_t partitionDataSize) override;
    void appendMultiPartitionEpilogue() override;
    void appendEventForProfilingAllWalkers(Event *event, void **syncCmdBuffer, CommandToPatchContainer *outTimeStampSyncCmds, bool beforeWalker, bool singlePacketEvent, bool skipAddingEventToResidency, bool copyOperation);
    ze_result_t addEventsToCmdList(uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CommandToPatchContainer *outWaitCmds,
                                   bool relaxedOrderingAllowed, bool trackDependencies, bool waitForImplicitInOrderDependency, bool skipAddingWaitEventsToResidency, bool dualStreamCopyOffloadOperation);

    MOCKABLE_VIRTUAL void appendSynchronizedDispatchInitializationSection();
    MOCKABLE_VIRTUAL void appendSynchronizedDispatchCleanupSection();
    ze_result_t appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t reserveSpace(size_t size, void **ptr) override;
    ze_result_t reset() override;
    size_t getReserveSshSize();
    void patchInOrderCmds() override;
    MOCKABLE_VIRTUAL bool handleCounterBasedEventOperations(Event *signalEvent, bool skipAddingEventToResidency);
    bool isCbEventBoundToCmdList(Event *event) const;
    bool kernelMemoryPrefetchEnabled() const override;
    void assignInOrderExecInfoToEvent(Event *event);

  protected:
    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
                                                              uint64_t dstOffset, void *srcPtr,
                                                              NEO::GraphicsAllocation *srcPtrAlloc,
                                                              uint64_t srcOffset, uint64_t size,
                                                              uint64_t elementSize, Builtin builtin,
                                                              Event *signalEvent,
                                                              bool isStateless,
                                                              CmdListKernelLaunchParams &launchParams);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                                      NEO::GraphicsAllocation *dstPtrAlloc,
                                                      uint64_t dstOffset, uintptr_t srcPtr,
                                                      NEO::GraphicsAllocation *srcPtrAlloc,
                                                      uint64_t srcOffset,
                                                      uint64_t size,
                                                      Event *signalEvent);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyBlitRegion(AlignedAllocationData *srcAllocationData,
                                                            AlignedAllocationData *dstAllocationData,
                                                            ze_copy_region_t srcRegion,
                                                            ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                                            size_t srcRowPitch, size_t srcSlicePitch,
                                                            size_t dstRowPitch, size_t dstSlicePitch,
                                                            const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                                            Event *signalEvent,
                                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch, bool dualStreamCopyOffload);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                                          Builtin builtin, const ze_copy_region_t *dstRegion,
                                                          uint32_t dstPitch, size_t dstOffset,
                                                          const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                                          size_t srcOffset, Event *signalEvent,
                                                          uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch);

    MOCKABLE_VIRTUAL ze_result_t appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation, AlignedAllocationData *srcAlignedAllocation,
                                                          Builtin builtin, const ze_copy_region_t *dstRegion,
                                                          uint32_t dstPitch, uint32_t dstSlicePitch, size_t dstOffset,
                                                          const ze_copy_region_t *srcRegion, uint32_t srcPitch,
                                                          uint32_t srcSlicePitch, size_t srcOffset,
                                                          Event *signalEvent, uint32_t numWaitEvents,
                                                          ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch);

    MOCKABLE_VIRTUAL ze_result_t appendBlitFill(void *ptr, const void *pattern,
                                                size_t patternSize, size_t size,
                                                Event *signalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams);

    MOCKABLE_VIRTUAL ze_result_t appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                                     NEO::GraphicsAllocation *dst,
                                                     const Vec3<size_t> &srcOffsets, const Vec3<size_t> &dstOffsets,
                                                     size_t srcRowPitch, size_t srcSlicePitch,
                                                     size_t dstRowPitch, size_t dstSlicePitch,
                                                     size_t bytesPerPixel, const Vec3<size_t> &copySize,
                                                     const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                                     Event *signalEvent, uint32_t numWaitEvents,
                                                     ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams);

    virtual ze_result_t appendLaunchKernelWithParams(Kernel *kernel,
                                                     const ze_group_count_t &threadGroupDimensions,
                                                     Event *event,
                                                     CmdListKernelLaunchParams &launchParams);
    MOCKABLE_VIRTUAL ze_result_t appendLaunchKernelSplit(Kernel *kernel,
                                                         const ze_group_count_t &threadGroupDimensions,
                                                         Event *event,
                                                         CmdListKernelLaunchParams &launchParams);

    ze_result_t appendUnalignedFillKernel(bool isStateless,
                                          uint32_t unalignedSize,
                                          const AlignedAllocationData &dstAllocation,
                                          const void *pattern,
                                          Event *signalEvent,
                                          CmdListKernelLaunchParams &launchParams);

    void appendWaitOnSingleEvent(Event *event, CommandToPatchContainer *outWaitCmds, bool relaxedOrderingAllowed, bool dualStreamCopyOffload, CommandToPatch::CommandType storedSemaphore);

    void appendSdiInOrderCounterSignalling(uint64_t baseGpuVa, uint64_t signalValue, bool copyOffloadOperation);

    ze_result_t prepareIndirectParams(const ze_group_count_t *threadGroupDimensions);
    void updateStreamPropertiesForRegularCommandLists(Kernel &kernel, bool isCooperative, const ze_group_count_t &threadGroupDimensions, bool isIndirect);
    void appendVfeStateCmdToPatch();
    void updateStreamPropertiesForFlushTaskDispatchFlags(Kernel &kernel, bool isCooperative, const ze_group_count_t &threadGroupDimensions, bool isIndirect);
    void updateStreamProperties(Kernel &kernel, bool isCooperative, const ze_group_count_t &threadGroupDimensions, bool isIndirect);
    void clearCommandsToPatch();

    size_t getTotalSizeForCopyRegion(const ze_copy_region_t *region, uint32_t pitch, uint32_t slicePitch);
    bool isAppendSplitNeeded(void *dstPtr, const void *srcPtr, size_t size, NEO::TransferDirection &directionOut);
    bool isAppendSplitNeeded(NEO::MemoryPool dstPool, NEO::MemoryPool srcPool, size_t size, NEO::TransferDirection &directionOut);

    void applyMemoryRangesBarrier(uint32_t numRanges, const size_t *pRangeSizes,
                                  const void **pRanges);

    ze_result_t programSyncBuffer(Kernel &kernel, NEO::Device &device, const ze_group_count_t &threadGroupDimensions, size_t &patchIndex);
    void programRegionGroupBarrier(Kernel &kernel, const ze_group_count_t &threadGroupDimensions, size_t localRegionSize, size_t &patchIndex);
    void appendWriteKernelTimestamp(Event *event, CommandToPatchContainer *outTimeStampSyncCmds, bool beforeWalker, bool maskLsb, bool workloadPartition, bool copyOperation);
    void adjustWriteKernelTimestamp(uint64_t globalAddress, uint64_t contextAddress, uint64_t baseAddress, CommandToPatchContainer *outTimeStampSyncCmds, bool workloadPartition, bool copyOperation);
    void appendEventForProfiling(Event *event, CommandToPatchContainer *outTimeStampSyncCmds, bool beforeWalker, bool skipBarrierForEndProfiling, bool skipAddingEventToResidency, bool copyOperation);
    void appendEventForProfilingCopyCommand(Event *event, bool beforeWalker);
    void appendSignalEventPostWalker(Event *event, void **syncCmdBuffer, CommandToPatchContainer *outTimeStampSyncCmds, bool skipBarrierForEndProfiling, bool skipAddingEventToResidency, bool copyOperation);
    void programStateBaseAddress(NEO::CommandContainer &container, bool useSbaProperties);
    virtual void programStateBaseAddressHook(size_t cmdBufferOffset, bool surfaceBaseAddressModify) {
    }
    void appendComputeBarrierCommand();
    NEO::PipeControlArgs createBarrierFlags();
    void appendMultiTileBarrier(NEO::Device &neoDevice);
    void appendDispatchOffsetRegister(bool workloadPartitionEvent, bool beforeProfilingCmds);
    size_t estimateBufferSizeMultiTileBarrier(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    uint64_t getInputBufferSize(NEO::ImageType imageType, uint32_t bufferRowPitch, uint32_t bufferSlicePitch, const ze_image_region_t *region);
    MOCKABLE_VIRTUAL AlignedAllocationData getAlignedAllocationData(Device *device, bool sharedSystemEnabled, const void *buffer, uint64_t bufferSize, bool hostCopyAllowed, bool copyOffload);
    size_t getAllocationOffsetForAppendBlitFill(void *ptr, NEO::GraphicsAllocation &gpuAllocation);
    uint32_t getRegionOffsetForAppendMemoryCopyBlitRegion(AlignedAllocationData *allocationData);
    void handlePostSubmissionState();

    MOCKABLE_VIRTUAL void setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, bool useAdditionalTimestamp);

    void setupFillKernelArguments(size_t baseOffset,
                                  size_t patternSize,
                                  size_t dstSize,
                                  CmdListFillKernelArguments &outArguments,
                                  Kernel *kernel);
    bool compactL3FlushEvent(bool dcFlush) const {
        return this->compactL3FlushEventPacket && dcFlush;
    }
    bool eventSignalPipeControl(bool splitKernel, bool dcFlush) const {
        return (this->pipeControlMultiKernelEventSync && splitKernel) ||
               compactL3FlushEvent(dcFlush);
    }
    MOCKABLE_VIRTUAL void allocateOrReuseKernelPrivateMemory(Kernel *kernel, uint32_t sizePerHwThread, NEO::PrivateAllocsToReuseContainer &privateAllocsToReuse);
    virtual void allocateOrReuseKernelPrivateMemoryIfNeeded(Kernel *kernel, uint32_t sizePerHwThread);
    CmdListEventOperation estimateEventPostSync(Event *event, uint32_t operations);
    void dispatchPostSyncCopy(uint64_t gpuAddress, uint32_t value, bool workloadPartition, void **outCmdBuffer);
    void dispatchPostSyncCompute(uint64_t gpuAddress, uint32_t value, bool workloadPartition, void **outCmdBuffer);
    void dispatchPostSyncCommands(const CmdListEventOperation &eventOperations, uint64_t gpuAddress, void **syncCmdBuffer, CommandToPatchContainer *outListCommands, uint32_t value, bool useLastPipeControl, bool signalScope, bool skipPartitionOffsetProgramming, bool copyOperation);
    void dispatchEventRemainingPacketsPostSyncOperation(Event *event, bool copyOperation);
    void dispatchEventPostSyncOperation(Event *event, void **syncCmdBuffer, CommandToPatchContainer *outListCommands, uint32_t value, bool omitFirstOperation, bool useMax, bool useLastPipeControl,
                                        bool skipPartitionOffsetProgramming, bool copyOperation);

    bool isAllocationImported(NEO::GraphicsAllocation *gpuAllocation, NEO::SVMAllocsManager *svmManager) const;
    static constexpr bool checkIfAllocationImportedRequired();

    bool isKernelUncachedMocsRequired(bool kernelState);

    bool isUsingSystemAllocation(const NEO::AllocationType &allocType) const {
        return ((allocType == NEO::AllocationType::bufferHostMemory) ||
                (allocType == NEO::AllocationType::svmCpu) ||
                (allocType == NEO::AllocationType::externalHostPtr));
    }

    void postInitComputeSetup();
    NEO::PreemptionMode obtainKernelPreemptionMode(Kernel *kernel);
    virtual bool isRelaxedOrderingDispatchAllowed(uint32_t numWaitEvents, bool copyOffload) { return false; }
    virtual void setupFlushMethod(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {}
    bool canSkipInOrderEventWait(Event &event, bool ignorCbEventBoundToCmdList) const;
    bool handleInOrderImplicitDependencies(bool relaxedOrderingAllowed, bool dualStreamCopyOffloadOperation);
    bool isQwordInOrderCounter() const { return GfxFamily::isQwordInOrderCounter; }
    bool isInOrderNonWalkerSignalingRequired(const Event *event) const;
    bool hasInOrderDependencies() const;
    void appendFullSynchronizedDispatchInit();
    void addPatchScratchAddressInImplicitArgs(CommandsToPatch &commandsToPatch, NEO::EncodeDispatchKernelArgs &args, const NEO::KernelDescriptor &kernelDescriptor, bool kernelNeedsImplicitArgs);
    size_t addCmdForPatching(std::shared_ptr<NEO::InOrderExecInfo> *externalInOrderExecInfo, void *cmd1, void *cmd2, uint64_t counterValue, NEO::InOrderPatchCommandHelpers::PatchCmdType patchCmdType);
    uint64_t getInOrderIncrementValue() const;
    bool isSkippingInOrderBarrierAllowed(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) const;
    void encodeMiFlush(uint64_t immediateDataGpuAddress, uint64_t immediateData, NEO::MiFlushArgs &args);

    void updateInOrderExecInfo(size_t inOrderPatchIndex, std::shared_ptr<NEO::InOrderExecInfo> *inOrderExecInfo, bool disablePatchingFlag);
    void disablePatching(size_t inOrderPatchIndex);
    void enablePatching(size_t inOrderPatchIndex);

    void appendCopyOperationFence(Event *signalEvent, NEO::GraphicsAllocation *srcAllocation, NEO::GraphicsAllocation *dstAllocation, bool copyEngineOperation);
    bool isDeviceToHostCopyEventFenceRequired(Event *signalEvent) const;
    bool isDeviceToHostBcsCopy(NEO::GraphicsAllocation *srcAllocation, NEO::GraphicsAllocation *dstAllocation, bool copyEngineOperation) const;
    bool singleEventPacketRequired(bool inputSinglePacketEventRequest) const;
    void programEventL3Flush(Event *event);
    virtual ze_result_t flushInOrderCounterSignal(bool waitOnInOrderCounterRequired) { return ZE_RESULT_SUCCESS; };
    bool isCopyOffloadAllowed(const NEO::GraphicsAllocation &srcAllocation, const NEO::GraphicsAllocation &dstAllocation) const;
    void setAdditionalKernelLaunchParams(CmdListKernelLaunchParams &launchParams, Kernel &kernel) const;
    void dispatchInOrderPostOperationBarrier(Event *signalOperation, bool dcFlushRequired, bool copyOperation);
    NEO::GraphicsAllocation *getDeviceCounterAllocForResidency(NEO::GraphicsAllocation *counterDeviceAlloc);
    bool isHighPriorityImmediateCmdList() const;
    void prefetchKernelMemory(NEO::LinearStream &cmdStream, const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, size_t iohOffset, CommandToPatchContainer *outListCommands, uint64_t cmdId);
    virtual void addKernelIsaMemoryPrefetchPadding(NEO::LinearStream &cmdStream, const Kernel &kernel, uint64_t cmdId) {}
    virtual void addKernelIndirectDataMemoryPrefetchPadding(NEO::LinearStream &cmdStream, const Kernel &kernel, uint64_t cmdId) {}
    virtual uint64_t getPrefetchCmdId() const { return std::numeric_limits<uint64_t>::max(); }
    virtual uint32_t getIohSizeForPrefetch(const Kernel &kernel, uint32_t reserveExtraSpace) const;
    virtual void ensureCmdBufferSpaceForPrefetch() {}

    NEO::InOrderPatchCommandsContainer<GfxFamily> inOrderPatchCmds;

    bool latestOperationHasOptimizedCbEvent = false;
    bool latestOperationRequiredNonWalkerInOrderCmdsChaining = false;
    bool duplicatedInOrderCounterStorageEnabled = false;
    bool inOrderAtomicSignalingEnabled = false;
    bool allowCbWaitEventsNoopDispatch = false;
    bool copyOperationFenceSupported = false;
    bool implicitSynchronizedDispatchForCooperativeKernelsAllowed = false;
    bool useAdditionalBlitProperties = false;
    bool isPostImageWriteFlushRequired = false;
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListProductFamily;

} // namespace L0

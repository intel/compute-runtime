/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"

namespace L0 {
namespace MCL {
struct MutableComputeWalker;

struct MutableAppendLaunchKernelWithParams {
    Variable *groupCountVariable = nullptr;
    Variable *groupSizeVariable = nullptr;
    Variable *globalOffsetVariable = nullptr;
    Variable *lastSlmArgumentVariable = nullptr;
    MutableKernel *currentMutableKernel = nullptr;
    uint32_t maxKernelGroupScratch[2] = {0, 0};
    uint32_t maxKernelGroupIndirectHeap = 0;
    uint32_t extraPayloadSpaceForKernelGroup = 0;
    ze_mutable_command_exp_flags_t mutationFlags = 0;
    NEO::RequiredPartitionDim requiredPartitionDimFromApi = NEO::RequiredPartitionDim::none;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrderFromApi = NEO::RequiredDispatchWalkOrder::none;

    bool kernelArgumentMutation = false;
    bool kernelMutation = false;
    bool isCooperativeFromApi = false;
};

struct MutableAppendEvents {
    CommandToPatchInCmdList signalCmd = {PatchSignalEventPostSyncPipeControl{}};
    CommandToPatchContainer *mutableCmdPatchlistContainer = nullptr;

    bool waitEvents = false;
    bool signalEvent = false;
    bool l3FlushEventSyncCmd = false;
    bool l3FlushEventTimestampSyncCmds = false;
    bool counterBasedEvent = false;
    bool counterBasedTimestampEvent = false;
    bool l3FlushEvent = false;
    bool eventInsideInOrder = false;
    bool inOrderIncrementEvent = false;
    bool omitWaitEventResidency = false;
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MutableCommandListCoreFamily : public MutableCommandListImp, public CommandListCoreFamily<gfxCoreFamily> {
    MutableCommandListCoreFamily(uint32_t numIddsPerBlock) : MutableCommandListImp(this), CommandListCoreFamily<gfxCoreFamily>(numIddsPerBlock) {}

    using GfxFamily = typename CommandListCoreFamily<gfxCoreFamily>::GfxFamily;

    void *asMutable() override { return static_cast<MutableCommandList *>(this); }
    ze_result_t initialize(Device *device, NEO::EngineGroupType engineGroupType, ze_command_list_flags_t flags) override;
    ze_result_t appendLaunchKernelWithParams(Kernel *kernel,
                                             const ze_group_count_t &threadGroupDimensions,
                                             Event *event,
                                             CmdListKernelLaunchParams &launchParams) override;
    ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                   const ze_group_count_t &threadGroupDimensions,
                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   CmdListKernelLaunchParams &launchParams) override;
    ze_result_t close() override;
    ze_result_t reset() override;
    ze_result_t appendVariableLaunchKernel(Kernel *kernel,
                                           Variable *groupCountVariable,
                                           Event *signalEvent,
                                           uint32_t numWaitEvents,
                                           ze_event_handle_t *waitEvents) override;
    ze_result_t appendJump(Label *label, const InterfaceOperandDescriptor *condition) override;
    ze_result_t appendSetPredicate(NEO::MiPredicateType predicateType) override;
    ze_result_t appendMILoadRegVariable(MclAluReg reg, Variable *variable) override;
    ze_result_t appendMIStoreRegVariable(MclAluReg reg, Variable *variable) override;
    ze_result_t appendMILoadRegImm(MclAluReg reg, uint32_t value) override;
    ze_result_t appendMILoadRegReg(MclAluReg dstReg, MclAluReg srcReg) override;
    ze_result_t appendMILoadRegMem(MclAluReg reg, uint64_t address) override;
    ze_result_t appendMIStoreRegMem(MclAluReg reg, uint64_t address) override;
    ze_result_t appendMIMath(void *aluArray, size_t aluCount) override;

    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents, CmdListWaitEventParameters &waitEventsParameters) override;
    ze_result_t appendMemoryRangesBarrier(uint32_t numRanges, const size_t *pRangeSizes,
                                          const void **pRanges,
                                          ze_event_handle_t hSignalEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents,
                                          CmdListWaitEventParameters &waitEventParams) override;
    ze_result_t appendImageCopyFromMemoryExt(ze_image_handle_t hDstImage, const void *srcptr,
                                             const ze_image_region_t *pDstRegion,
                                             uint32_t srcRowPitch, uint32_t srcSlicePitch,
                                             ze_event_handle_t hEvent, uint32_t numWaitEvents,
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
    ze_result_t appendMemoryCopy(void *dstptr, const void *srcptr, size_t size,
                                 ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
                                            size_t size, ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
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
    ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                 size_t patternSize, size_t size, ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) override;
    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CmdListWaitEventParameters &waitEventParams) override;
    ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                           CmdListWaitEventParameters &waitEventParams) override;
    ze_result_t appendHostFunction(ze_host_function_callback_t pHostFunction,
                                   void *pUserData,
                                   const void *pNext,
                                   ze_event_handle_t hSignalEvent,
                                   uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents,
                                   CmdListHostFunctionParameters &parameters) override;

    void programStateBaseAddressHook(size_t cmdBufferOffset, bool surfaceBaseAddressModify) override;
    void setBufferSurfaceState(void *address, NEO::GraphicsAllocation *alloc, Variable *variable) override;

    MutableComputeWalker *getCommandWalker(CommandBufferOffset offsetToWalkerCommand, uint8_t indirectOffset, uint8_t scratchOffset) override;
    uint32_t getInlineDataSize() const;

    void switchCounterBasedEvents(uint64_t inOrderExecBaseSignalValue, uint32_t inOrderAllocationOffset, Event *newEvent) override;
    bool isCbEventBoundToCmdList(Event *event) const override {
        return CommandListCoreFamily<gfxCoreFamily>::isCbEventBoundToCmdList(event);
    }

    NEO::GraphicsAllocation *getDeviceCounterAllocForResidency(NEO::GraphicsAllocation *counterDeviceAlloc) override {
        return CommandListCoreFamily<gfxCoreFamily>::getDeviceCounterAllocForResidency(counterDeviceAlloc);
    }

    bool isQwordInOrderCounter() const override {
        return CommandListCoreFamily<gfxCoreFamily>::isQwordInOrderCounter();
    }

    void storeKernelArgumentAndDispatchVariables(MutableAppendLaunchKernelWithParams &mutableParams,
                                                 CmdListKernelLaunchParams &launchParams,
                                                 Kernel *kernel,
                                                 KernelVariableDescriptor *kernelVariables);
    void storeSignalEventVariable(MutableAppendEvents &mutableEventParams,
                                  CmdListKernelLaunchParams &launchParams,
                                  Event *event);

    void captureCounterBasedWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                              std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                              std::vector<MutableLoadRegisterImm *> &variableLoadRegisterImmList);
    void captureRegularWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                         std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList);
    void captureCounterBasedTimestampSignalEventCommands(SignalEventVariableDescriptor &currentMutableSignalEvent,
                                                         std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                                         std::vector<MutableStoreDataImm *> &variableStoreDataImmList);
    void captureStandaloneTimestampSignalEventCommands(std::vector<MutableStoreRegisterMem *> &variableStoreRegisterMem);

    ze_result_t captureKernelGroupVariablesAndCommandView(MutableKernel *mutableKernel,
                                                          void *batchBufferWalker,
                                                          const ze_group_count_t &threadGroupDimensions,
                                                          Event *event,
                                                          MutableAppendLaunchKernelWithParams &parentMutableAppendLaunchParams);
    void storeWaitEventsVariables(uint32_t numWaitEvents,
                                  ze_event_handle_t *phWaitEvents,
                                  MutableAppendEvents &mutableEventParams);
    void processWaitEventVariables(uint32_t numWaitEvents);
    void clearMutableAppendData();

    void updateScratchAddress(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) override;
    void updateCmdListScratchPatchCommand(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) override;
    uint64_t getCurrentScratchPatchAddress(size_t scratchAddressPatchIndex) const override;
    void updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) override;
    size_t createNewCmdListNoopPatchData(void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) override;
    void fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset, uint64_t &gpuAddress) override;
    void disableAddressNoopPatch(size_t noopPatchIndex) override;
    uint64_t getPrefetchCmdId() const override;
    void updateKernelMemoryPrefetch(const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, const PatchPrefetchKernelMemory &cmdToPatch, uint64_t cmdId) override;
    uint32_t getIohSizeForPrefetch(const Kernel &kernel, uint32_t reserveExtraSpace) const override;
    MutableKernelGroup *getKernelGroupForPrefetch(uint64_t cmdId) const;
    size_t ensureCmdBufferSpaceForPrefetch() override;
};
template <PRODUCT_FAMILY gfxProductFamily>
struct MutableCommandListProductFamily;
} // namespace MCL
} // namespace L0

/*
 * Copyright (C) 2025 Intel Corporation
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
    uint32_t localRegionSizeFromApi = NEO::localRegionSizeParamNotSet;

    bool kernelArgumentMutation = false;
    bool kernelMutation = false;
    bool isCooperativeFromApi = false;
};

struct MutableAppendLaunchKernelEvents {
    CommandToPatch signalCmd;

    size_t currentSignalEventDescriptorIndex = std::numeric_limits<size_t>::max();

    bool waitEvents = false;
    bool l3FlushEventSyncCmd = false;
    bool l3FlushEventTimestampSyncCmds = false;
    bool counterBasedEvent = false;
    bool counterBasedTimestampEvent = false;
    bool l3FlushEvent = false;
    bool eventInsideInOrder = false;
    bool inOrderIncrementEvent = false;
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

    void updateInOrderExecInfo(size_t inOrderPatchIndex, std::shared_ptr<NEO::InOrderExecInfo> *inOrderExecInfo, bool disablePatchingFlag) override {
        CommandListCoreFamily<gfxCoreFamily>::updateInOrderExecInfo(inOrderPatchIndex, inOrderExecInfo, disablePatchingFlag);
    }
    void disablePatching(size_t inOrderPatchIndex) override {
        CommandListCoreFamily<gfxCoreFamily>::disablePatching(inOrderPatchIndex);
    }
    void enablePatching(size_t inOrderPatchIndex) override {
        CommandListCoreFamily<gfxCoreFamily>::enablePatching(inOrderPatchIndex);
    }

    void storeKernelArgumentAndDispatchVariables(MutableAppendLaunchKernelWithParams &mutableParams,
                                                 CmdListKernelLaunchParams &launchParams,
                                                 Kernel *kernel,
                                                 MutationVariables *variableDescriptors,
                                                 ze_mutable_command_exp_flags_t mutableFlags);
    void storeSignalEventVariable(MutableAppendLaunchKernelEvents &mutableEventParams,
                                  CmdListKernelLaunchParams &launchParams,
                                  Event *event,
                                  MutationVariables *variableDescriptors,
                                  ze_mutable_command_exp_flags_t mutableFlags);

    void captureCounterBasedWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                              std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                              std::vector<MutableLoadRegisterImm *> &variableLoadRegisterImmList);
    void captureRegularWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                         std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList);
    void captureCounterBasedTimestampSignalEventCommands(MutableVariableDescriptor &currentMutableSignalEvent,
                                                         std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                                         std::vector<MutableStoreDataImm *> &variableStoreDataImmList);
    void captureStandaloneTimestampSignalEventCommands(std::vector<MutableStoreRegisterMem *> &variableStoreRegisterMem);

    ze_result_t captureKernelGroupVariablesAndCommandView(MutableKernel *mutableKernel,
                                                          void *batchBufferWalker,
                                                          const ze_group_count_t &threadGroupDimensions,
                                                          Event *event,
                                                          MutableAppendLaunchKernelWithParams &parentMutableAppendLaunchParams);

    void updateScratchAddress(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) override;
    void updateCmdListScratchPatchCommand(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) override;
    uint64_t getCurrentScratchPatchAddress(size_t scratchAddressPatchIndex) const override;
    void updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset) override;
    size_t createNewCmdListNoopPatchData(void *newCpuPtr, size_t newPatchSize, size_t newOffset) override;
    void fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset) override;
    void addKernelIsaMemoryPrefetchPadding(NEO::LinearStream &cmdStream, const Kernel &kernel, uint64_t cmdId) override;
    void addKernelIndirectDataMemoryPrefetchPadding(NEO::LinearStream &cmdStream, const Kernel &kernel, uint64_t cmdId) override;
    uint64_t getPrefetchCmdId() const override;
    void updateKernelMemoryPrefetch(const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, const CommandToPatch &cmdToPatch, uint64_t cmdId) override;
    uint32_t getIohSizeForPrefetch(const Kernel &kernel, uint32_t reserveExtraSpace) const override;
    MutableKernelGroup *getKernelGroupForPrefetch(uint64_t cmdId) const;
    void ensureCmdBufferSpaceForPrefetch() override;
};
template <PRODUCT_FAMILY gfxProductFamily>
struct MutableCommandListProductFamily;
} // namespace MCL
} // namespace L0

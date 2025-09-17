/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"

namespace NEO {
class ScratchSpaceController;
} // namespace NEO

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandQueueHw : public CommandQueueImp {
    using CommandQueueImp::CommandQueueImp;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    ze_result_t createFence(const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) override;
    ze_result_t executeCommandLists(uint32_t numCommandLists,
                                    ze_command_list_handle_t *phCommandLists,
                                    ze_fence_handle_t hFence, bool performMigration,
                                    NEO::LinearStream *parentImmediateCommandlistLinearStream,
                                    std::unique_lock<std::mutex> *outerLockForIndirect) override;

    void programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed, NEO::StreamProperties *streamProperties);
    size_t estimateStateBaseAddressCmdSize();
    MOCKABLE_VIRTUAL void programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSlot0Size, NEO::LinearStream &commandStream, NEO::StreamProperties &streamProperties);

    MOCKABLE_VIRTUAL size_t estimateFrontEndCmdSizeForMultipleCommandLists(bool &isFrontEndStateDirty, CommandList *commandList,
                                                                           NEO::StreamProperties &csrState,
                                                                           const NEO::StreamProperties &cmdListRequired,
                                                                           const NEO::StreamProperties &cmdListFinal,
                                                                           NEO::StreamProperties &requiredState,
                                                                           bool &propertyDirty,
                                                                           bool &frontEndReturnPoint);
    size_t estimateFrontEndCmdSize();
    size_t estimateFrontEndCmdSize(bool isFrontEndDirty);

    void programPipelineSelectIfGpgpuDisabled(NEO::LinearStream &commandStream);

    MOCKABLE_VIRTUAL void handleScratchSpace(NEO::HeapContainer &heapContainer,
                                             NEO::ScratchSpaceController *scratchController,
                                             NEO::GraphicsAllocation *globalStatelessAllocation,
                                             bool &gsbaState, bool &frontEndState,
                                             uint32_t perThreadScratchSpaceSlot0Size,
                                             uint32_t perThreadScratchSpaceSlot1Size);

    bool getPreemptionCmdProgramming() override;
    void patchCommands(CommandList &commandList, uint64_t scratchAddress, bool patchNewScratchController,
                       void **patchPreambleBuffer);

  protected:
    struct CommandListExecutionContext {

        CommandListExecutionContext() {}

        CommandListExecutionContext(ze_command_list_handle_t *commandListHandles,
                                    uint32_t numCommandLists,
                                    NEO::PreemptionMode contextPreemptionMode,
                                    Device *device,
                                    NEO::ScratchSpaceController *scratchSpaceController,
                                    NEO::GraphicsAllocation *globalStatelessAllocation,
                                    bool debugEnabled,
                                    bool programActivePartitionConfig,
                                    bool performMigration,
                                    bool sipSent);

        inline bool isNEODebuggerActive(Device *device);

        NEO::StreamProperties cmdListBeginState{};
        uint64_t scratchGsba = 0;
        uint64_t childGpuAddressPositionBeforeDynamicPreamble = 0;
        uint64_t currentGpuAddressForChainedBbStart = 0;

        size_t spaceForResidency = 10;
        size_t bufferSpaceForPatchPreamble = 0;
        size_t totalNoopSpaceForPatchPreamble = 0;
        CommandList *firstCommandList = nullptr;
        CommandList *lastCommandList = nullptr;
        void *currentPatchForChainedBbStart = nullptr;
        void *currentPatchPreambleBuffer = nullptr;
        NEO::ScratchSpaceController *scratchSpaceController = nullptr;
        NEO::GraphicsAllocation *globalStatelessAllocation = nullptr;
        std::unique_lock<std::mutex> *outerLockForIndirect = nullptr;

        NEO::PreemptionMode preemptionMode{};
        NEO::PreemptionMode statePreemption{};
        uint32_t perThreadScratchSpaceSlot0Size = 0;
        uint32_t perThreadScratchSpaceSlot1Size = 0;
        uint32_t totalActiveScratchPatchElements = 0;
        UnifiedMemoryControls unifiedMemoryControls{};

        bool anyCommandListWithCooperativeKernels = false;
        bool anyCommandListRequiresDisabledEUFusion = false;
        bool cachedMOCSAllowed = true;
        bool containsAnyRegularCmdList = false;
        bool gsbaStateDirty = false;
        bool frontEndStateDirty = false;
        const bool isPreemptionModeInitial{false};
        bool isDevicePreemptionModeMidThread{};
        bool isDebugEnabled{};
        bool stateSipRequired{};
        bool isProgramActivePartitionConfigRequired{};
        bool isMigrationRequested{};
        bool isDirectSubmissionEnabled{};
        bool isDispatchTaskCountPostSyncRequired{};
        bool hasIndirectAccess{};
        bool rtDispatchRequired = false;
        bool globalInit = false;
        bool lockScratchController = false;
        bool cmdListScratchAddressPatchingEnabled = false;
        bool containsParentImmediateStream = false;
    };

    inline void processMemAdviseOperations(CommandList *commandList);

    ze_result_t executeCommandListsRegularHeapless(CommandListExecutionContext &ctx,
                                                   uint32_t numCommandLists,
                                                   ze_command_list_handle_t *commandListHandles,
                                                   ze_fence_handle_t hFence,
                                                   NEO::LinearStream *parentImmediateCommandlistLinearStream);

    MOCKABLE_VIRTUAL ze_result_t executeCommandListsRegular(CommandListExecutionContext &ctx,
                                                            uint32_t numCommandLists,
                                                            ze_command_list_handle_t *commandListHandles,
                                                            ze_fence_handle_t hFence,
                                                            NEO::LinearStream *parentImmediateCommandlistLinearStream);
    inline ze_result_t executeCommandListsCopyOnly(CommandListExecutionContext &ctx,
                                                   uint32_t numCommandLists,
                                                   ze_command_list_handle_t *phCommandLists,
                                                   ze_fence_handle_t hFence,
                                                   NEO::LinearStream *parentImmediateCommandlistLinearStream);
    inline size_t computeDebuggerCmdsSize(const CommandListExecutionContext &ctx);
    inline size_t computePreemptionSizeForCommandList(CommandListExecutionContext &ctx,
                                                      CommandList *commandList,
                                                      bool &dirtyState);
    inline ze_result_t setupCmdListsAndContextParams(CommandListExecutionContext &ctx,
                                                     ze_command_list_handle_t *phCommandLists,
                                                     uint32_t numCommandLists,
                                                     ze_fence_handle_t hFence,
                                                     NEO::LinearStream *parentImmediateCommandlistLinearStream);
    MOCKABLE_VIRTUAL bool isDispatchTaskCountPostSyncRequired(ze_fence_handle_t hFence, bool containsAnyRegularCmdList, bool containsParentImmediateStream) const;
    inline size_t estimateLinearStreamSizeInitial(CommandListExecutionContext &ctx);
    size_t estimateStreamSizeForExecuteCommandListsRegularHeapless(CommandListExecutionContext &ctx,
                                                                   uint32_t numCommandLists,
                                                                   ze_command_list_handle_t *commandListHandles,
                                                                   bool instructionCacheFlushRequired,
                                                                   bool stateCacheFlushRequired);
    inline size_t estimateCommandListSecondaryStart(CommandList *commandList);
    inline size_t estimateCommandListPrimaryStart(bool required);
    inline size_t estimateCommandListPatchPreamble(CommandListExecutionContext &ctx, uint32_t numCommandLists);
    inline size_t estimateCommandListPatchPreambleFrontEndCmd(CommandListExecutionContext &ctx, CommandList *commandList);
    inline void getCommandListPatchPreambleData(CommandListExecutionContext &ctx, CommandList *commandList);
    inline size_t estimateCommandListPatchPreambleWaitSync(CommandListExecutionContext &ctx, CommandList *commandList);
    inline size_t estimateTotalPatchPreambleData(CommandListExecutionContext &ctx);
    inline void retrivePatchPreambleSpace(CommandListExecutionContext &ctx, NEO::LinearStream &commandStream);
    inline void dispatchPatchPreambleEnding(CommandListExecutionContext &ctx);
    inline void dispatchPatchPreambleInOrderNoop(CommandListExecutionContext &ctx, CommandList *commandList);
    inline void dispatchPatchPreambleCommandListWaitSync(CommandListExecutionContext &ctx, CommandList *commandList);
    inline size_t estimateCommandListResidencySize(CommandList *commandList);
    inline void setFrontEndStateProperties(CommandListExecutionContext &ctx);
    inline void handleScratchSpaceAndUpdateGSBAStateDirtyFlag(CommandListExecutionContext &ctx);
    inline size_t estimateLinearStreamSizeComplementary(CommandListExecutionContext &ctx,
                                                        ze_command_list_handle_t *phCommandLists,
                                                        uint32_t numCommandLists);
    MOCKABLE_VIRTUAL ze_result_t makeAlignedChildStreamAndSetGpuBase(NEO::LinearStream &child, size_t requiredSize, CommandListExecutionContext &ctx);
    inline void getGlobalFenceAndMakeItResident();
    inline void getWorkPartitionAndMakeItResident();
    inline void getGlobalStatelessHeapAndMakeItResident(CommandListExecutionContext &ctx);
    inline void getTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(NEO::LinearStream &commandStream);
    inline void makeSbaTrackingBufferResidentIfL0DebuggerEnabled(bool isDebugEnabled);
    inline void programCommandQueueDebugCmdsForDebuggerIfEnabled(bool isDebugEnabled, NEO::LinearStream &commandStream);
    inline void programStateBaseAddressWithGsbaIfDirty(CommandListExecutionContext &ctx,
                                                       ze_command_list_handle_t hCommandList,
                                                       NEO::LinearStream &commandStream);
    inline void programCsrBaseAddressIfPreemptionModeInitial(bool isPreemptionModeInitial, NEO::LinearStream &commandStream);
    inline void programStateSip(bool isStateSipRequired, NEO::LinearStream &commandStream);
    inline void updateOneCmdListPreemptionModeAndCtxStatePreemption(NEO::LinearStream &commandStream,
                                                                    CommandListRequiredStateChange &cmdListRequired);
    inline void makePreemptionAllocationResidentForModeMidThread(bool isDevicePreemptionModeMidThread);
    inline void makeSipIsaResidentIfSipKernelUsed(CommandListExecutionContext &ctx);
    inline void makeDebugSurfaceResidentIfNEODebuggerActive(bool isNEODebuggerActive);
    inline void makeCsrTagAllocationResident();
    inline void makeRayTracingBufferResident(NEO::GraphicsAllocation *rtBuffer);

    inline void programActivePartitionConfig(bool isProgramActivePartitionConfigRequired, NEO::LinearStream &commandStream);
    inline void programOneCmdListFrontEndIfDirty(CommandListExecutionContext &ctx,
                                                 NEO::LinearStream &commandStream,
                                                 CommandListRequiredStateChange &cmdListRequiredState);
    inline void programOneCmdListBatchBufferStart(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx);
    inline void programOneCmdListBatchBufferStartPrimaryBatchBuffer(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx);
    inline void programOneCmdListBatchBufferStartSecondaryBatchBuffer(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx);
    inline void programLastCommandListReturnBbStart(
        NEO::LinearStream &commandStream,
        CommandListExecutionContext &ctx);
    inline void programFrontEndAndClearDirtyFlag(bool shouldFrontEndBeProgrammed,
                                                 CommandListExecutionContext &ctx,
                                                 NEO::LinearStream &commandStream,
                                                 NEO::StreamProperties &csrState);
    inline void collectPrintfContentsFromCommandsList(CommandList *commandList);
    inline void migrateSharedAllocationsIfRequested(bool isMigrationRequested, CommandList *commandList);
    inline void prefetchMemoryToDeviceAssociatedWithCmdList(CommandList *commandList);
    inline void assignCsrTaskCountToFenceIfAvailable(ze_fence_handle_t hFence);
    inline void dispatchTaskCountPostSyncRegular(bool isDispatchTaskCountPostSyncRequired, NEO::LinearStream &commandStream);
    inline void dispatchTaskCountPostSyncByMiFlushDw(bool isDispatchTaskCountPostSyncRequired, bool fenceRequired, NEO::LinearStream &commandStream);
    NEO::SubmissionStatus prepareAndSubmitBatchBuffer(CommandListExecutionContext &ctx, NEO::LinearStream &innerCommandStream);

    inline void cleanLeftoverMemory(NEO::LinearStream &outerCommandStream, NEO::LinearStream &innerCommandStream);
    inline void updateTaskCountAndPostSync(bool isDispatchTaskCountPostSyncRequired,
                                           uint32_t numCommandLists,
                                           ze_command_list_handle_t *commandListHandles);
    inline ze_result_t waitForCommandQueueCompletionAndCleanHeapContainer();
    inline ze_result_t handleSubmissionAndCompletionResults(NEO::SubmissionStatus submitRet, ze_result_t completionRet);
    inline size_t estimatePipelineSelectCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrState,
                                                                       const NEO::StreamProperties &cmdListRequired,
                                                                       const NEO::StreamProperties &cmdListFinal,
                                                                       bool &gpgpuEnabled,
                                                                       NEO::StreamProperties &requiredState,
                                                                       bool &propertyDirty);
    inline size_t estimatePipelineSelectCmdSize();
    inline void programOneCmdListPipelineSelect(NEO::LinearStream &commandStream,
                                                CommandListRequiredStateChange &cmdListRequired);

    inline size_t estimateScmCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrState,
                                                            bool &scmStateDirty,
                                                            const NEO::StreamProperties &cmdListRequired,
                                                            const NEO::StreamProperties &cmdListFinal,
                                                            NEO::StreamProperties &requiredState,
                                                            bool &propertyDirty);

    inline void programRequiredStateComputeModeForCommandList(NEO::LinearStream &commandStream,
                                                              CommandListRequiredStateChange &cmdListRequired);

    inline size_t estimateStateBaseAddressCmdDispatchSize(bool bindingTableBaseAddress);
    inline size_t estimateStateBaseAddressCmdSizeForMultipleCommandLists(bool &baseAddressStateDirty,
                                                                         NEO::HeapAddressModel commandListHeapAddressModel,
                                                                         NEO::StreamProperties &csrState,
                                                                         const NEO::StreamProperties &cmdListRequired,
                                                                         const NEO::StreamProperties &cmdListFinal,
                                                                         NEO::StreamProperties &requiredState,
                                                                         bool &propertyDirty);
    inline size_t estimateStateBaseAddressCmdSizeForGlobalStatelessCommandList(bool &baseAddressStateDirty,
                                                                               NEO::StreamProperties &csrState,
                                                                               const NEO::StreamProperties &cmdListRequired,
                                                                               const NEO::StreamProperties &cmdListFinal,
                                                                               NEO::StreamProperties &requiredState,
                                                                               bool &propertyDirty);
    inline size_t estimateStateBaseAddressCmdSizeForPrivateHeapCommandList(bool &baseAddressStateDirty,
                                                                           NEO::StreamProperties &csrState,
                                                                           const NEO::StreamProperties &cmdListRequired,
                                                                           const NEO::StreamProperties &cmdListFinal,
                                                                           NEO::StreamProperties &requiredState,
                                                                           bool &propertyDirty);
    inline size_t estimateStateBaseAddressDebugTracking();

    inline void programRequiredStateBaseAddressForCommandList(CommandListExecutionContext &ctx,
                                                              NEO::LinearStream &commandStream,
                                                              CommandListRequiredStateChange &cmdListRequired);
    inline void updateBaseAddressState(CommandList *lastCommandList);
    inline void updateDebugSurfaceState(CommandListExecutionContext &ctx);
    inline void patchCommands(CommandList &commandList, CommandListExecutionContext &ctx);
    void prepareInOrderCommandList(CommandListImp *commandList, CommandListExecutionContext &ctx);

    size_t alignedChildStreamPadding{};
};

} // namespace L0

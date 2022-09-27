/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/hw_info.h"
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
    ze_result_t destroy() override;
    ze_result_t executeCommandLists(uint32_t numCommandLists,
                                    ze_command_list_handle_t *phCommandLists,
                                    ze_fence_handle_t hFence, bool performMigration) override;
    ze_result_t executeCommands(uint32_t numCommands,
                                void *phCommands,
                                ze_fence_handle_t hFence) override;

    void programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed);
    size_t estimateStateBaseAddressCmdSize();
    MOCKABLE_VIRTUAL void programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSize, NEO::LinearStream &commandStream, NEO::StreamProperties &streamProperties);

    MOCKABLE_VIRTUAL size_t estimateFrontEndCmdSizeForMultipleCommandLists(bool &isFrontEndStateDirty, int32_t engineInstanced, CommandList *commandList,
                                                                           NEO::StreamProperties &csrStateCopy,
                                                                           const NEO::StreamProperties &cmdListRequired,
                                                                           const NEO::StreamProperties &cmdListFinal);
    size_t estimateFrontEndCmdSize();
    size_t estimateFrontEndCmdSize(bool isFrontEndDirty);

    void programPipelineSelectIfGpgpuDisabled(NEO::LinearStream &commandStream);

    MOCKABLE_VIRTUAL void handleScratchSpace(NEO::HeapContainer &heapContainer,
                                             NEO::ScratchSpaceController *scratchController,
                                             bool &gsbaState, bool &frontEndState,
                                             uint32_t perThreadScratchSpaceSize,
                                             uint32_t perThreadPrivateScratchSize);

    bool getPreemptionCmdProgramming() override;
    void patchCommands(CommandList &commandList, uint64_t scratchAddress);

  protected:
    struct CommandListExecutionContext {

        CommandListExecutionContext() {}

        CommandListExecutionContext(ze_command_list_handle_t *phCommandLists,
                                    uint32_t numCommandLists,
                                    NEO::PreemptionMode contextPreemptionMode,
                                    Device *device,
                                    bool debugEnabled,
                                    bool programActivePartitionConfig,
                                    bool performMigration);

        inline bool isNEODebuggerActive(Device *device);

        NEO::StreamProperties cmdListBeginState{};

        size_t spaceForResidency = 0;
        NEO::PreemptionMode preemptionMode{};
        NEO::PreemptionMode statePreemption{};
        uint32_t perThreadScratchSpaceSize = 0;
        uint32_t perThreadPrivateScratchSize = 0;
        int32_t engineInstanced = -1;
        UnifiedMemoryControls unifiedMemoryControls{};

        bool anyCommandListWithCooperativeKernels = false;
        bool anyCommandListWithoutCooperativeKernels = false;
        bool anyCommandListRequiresDisabledEUFusion = false;
        bool cachedMOCSAllowed = true;
        bool performMemoryPrefetch = false;
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
    };

    ze_result_t validateCommandListsParams(CommandListExecutionContext &ctx,
                                           ze_command_list_handle_t *phCommandLists,
                                           uint32_t numCommandLists);
    ze_result_t executeCommandListsRegular(CommandListExecutionContext &ctx,
                                           uint32_t numCommandLists,
                                           ze_command_list_handle_t *phCommandLists,
                                           ze_fence_handle_t hFence);
    inline ze_result_t executeCommandListsCopyOnly(CommandListExecutionContext &ctx,
                                                   uint32_t numCommandLists,
                                                   ze_command_list_handle_t *phCommandLists,
                                                   ze_fence_handle_t hFence);
    inline size_t computeDebuggerCmdsSize(const CommandListExecutionContext &ctx);
    inline size_t computePreemptionSize(CommandListExecutionContext &ctx,
                                        ze_command_list_handle_t *phCommandLists,
                                        uint32_t numCommandLists);
    inline void setupCmdListsAndContextParams(CommandListExecutionContext &ctx,
                                              ze_command_list_handle_t *phCommandLists,
                                              uint32_t numCommandLists,
                                              ze_fence_handle_t hFence);
    MOCKABLE_VIRTUAL bool isDispatchTaskCountPostSyncRequired(ze_fence_handle_t hFence, bool containsAnyRegularCmdList) const;
    inline size_t estimateLinearStreamSizeInitial(const CommandListExecutionContext &ctx,
                                                  ze_command_list_handle_t *phCommandLists,
                                                  uint32_t numCommandLists);
    inline void setFrontEndStateProperties(CommandListExecutionContext &ctx);
    inline void handleScratchSpaceAndUpdateGSBAStateDirtyFlag(CommandListExecutionContext &ctx);
    inline size_t estimateLinearStreamSizeComplementary(CommandListExecutionContext &ctx,
                                                        ze_command_list_handle_t *phCommandLists,
                                                        uint32_t numCommandLists);
    MOCKABLE_VIRTUAL ze_result_t makeAlignedChildStreamAndSetGpuBase(NEO::LinearStream &child, size_t requiredSize);
    inline void allocateGlobalFenceAndMakeItResident();
    inline void allocateWorkPartitionAndMakeItResident();
    inline void allocateTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(NEO::LinearStream &commandStream);
    inline void makeSbaTrackingBufferResidentIfL0DebuggerEnabled(bool isDebugEnabled);
    inline void programCommandQueueDebugCmdsForSourceLevelOrL0DebuggerIfEnabled(bool isDebugEnabled, NEO::LinearStream &commandStream);
    inline void programStateBaseAddressWithGsbaIfDirty(CommandListExecutionContext &ctx,
                                                       ze_command_list_handle_t hCommandList,
                                                       NEO::LinearStream &commandStream);
    inline void programCsrBaseAddressIfPreemptionModeInitial(bool isPreemptionModeInitial, NEO::LinearStream &commandStream);
    inline void programStateSip(bool isStateSipRequired, NEO::LinearStream &commandStream);
    inline void updateOneCmdListPreemptionModeAndCtxStatePreemption(CommandListExecutionContext &ctx,
                                                                    NEO::PreemptionMode commandListPreemption,
                                                                    NEO::LinearStream &commandStream);
    inline void makePreemptionAllocationResidentForModeMidThread(bool isDevicePreemptionModeMidThread);
    inline void makeSipIsaResidentIfSipKernelUsed(CommandListExecutionContext &ctx);
    inline void makeDebugSurfaceResidentIfNEODebuggerActive(bool isNEODebuggerActive);
    inline void makeCsrTagAllocationResident();
    inline void programActivePartitionConfig(bool isProgramActivePartitionConfigRequired, NEO::LinearStream &commandStream);
    inline void encodeKernelArgsBufferAndMakeItResident();
    inline void writeCsrStreamInlineIfLogicalStateHelperAvailable(NEO::LinearStream &commandStream);
    inline void programOneCmdListFrontEndIfDirty(CommandListExecutionContext &ctx,
                                                 NEO::LinearStream &commandStream, NEO::StreamProperties &csrState,
                                                 const NEO::StreamProperties &cmdListRequired, const NEO::StreamProperties &cmdListFinal);
    inline void programOneCmdListBatchBufferStart(CommandList *commandList, NEO::LinearStream &commandStream);
    inline void programOneCmdListBatchBufferStart(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx);
    inline void mergeOneCmdListPipelinedState(CommandList *commandList);
    inline void programFrontEndAndClearDirtyFlag(bool shouldFrontEndBeProgrammed,
                                                 CommandListExecutionContext &ctx,
                                                 NEO::LinearStream &commandStream,
                                                 NEO::StreamProperties &csrState);
    inline void collectPrintfContentsFromAllCommandsLists(ze_command_list_handle_t *phCommandLists, uint32_t numCommandLists);
    inline void migrateSharedAllocationsIfRequested(bool isMigrationRequested, ze_command_list_handle_t hCommandList);
    inline void prefetchMemoryIfRequested(bool &isMemoryPrefetchRequested);
    inline void programStateSipEndWA(bool isStateSipRequired, NEO::LinearStream &commandStream);
    inline void assignCsrTaskCountToFenceIfAvailable(ze_fence_handle_t hFence);
    inline void dispatchTaskCountPostSyncRegular(bool isDispatchTaskCountPostSyncRequired, NEO::LinearStream &commandStream);
    inline void dispatchTaskCountPostSyncByMiFlushDw(bool isDispatchTaskCountPostSyncRequired, NEO::LinearStream &commandStream);
    NEO::SubmissionStatus prepareAndSubmitBatchBuffer(CommandListExecutionContext &ctx, NEO::LinearStream &innerCommandStream);

    inline void cleanLeftoverMemory(NEO::LinearStream &outerCommandStream, NEO::LinearStream &innerCommandStream);
    inline void updateTaskCountAndPostSync(bool isDispatchTaskCountPostSyncRequired);
    inline ze_result_t waitForCommandQueueCompletionAndCleanHeapContainer();
    inline ze_result_t handleSubmissionAndCompletionResults(NEO::SubmissionStatus submitRet, ze_result_t completionRet);
    inline size_t estimatePipelineSelectCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrStateCopy,
                                                                       const NEO::StreamProperties &cmdListRequired,
                                                                       const NEO::StreamProperties &cmdListFinal,
                                                                       bool &gpgpuEnabled);
    inline size_t estimatePipelineSelectCmdSize();
    inline void programOneCmdListPipelineSelect(CommandList *commandList,
                                                NEO::LinearStream &commandStream,
                                                NEO::StreamProperties &csrState,
                                                const NEO::StreamProperties &cmdListRequired,
                                                const NEO::StreamProperties &cmdListFinal);

    inline size_t estimateScmCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrStateCopy,
                                                            const NEO::StreamProperties &cmdListRequired,
                                                            const NEO::StreamProperties &cmdListFinal);
    inline void programRequiredStateComputeModeForCommandList(CommandList *commandList,
                                                              NEO::LinearStream &commandStream,
                                                              NEO::StreamProperties &csrState,
                                                              const NEO::StreamProperties &cmdListRequired,
                                                              const NEO::StreamProperties &cmdListFinal);

    inline void updateBaseAddressState(CommandList *lastCommandList);

    size_t alignedChildStreamPadding{};
};

} // namespace L0

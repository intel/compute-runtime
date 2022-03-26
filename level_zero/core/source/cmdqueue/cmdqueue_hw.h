/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/residency_container.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"

#include "igfxfmid.h"

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

    void dispatchTaskCountPostSync(NEO::LinearStream &commandStream, const NEO::HardwareInfo &hwInfo);
    bool isDispatchTaskCountPostSyncRequired(ze_fence_handle_t hFence) const;

    void programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed);
    size_t estimateStateBaseAddressCmdSize();
    MOCKABLE_VIRTUAL void programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSize, NEO::LinearStream &commandStream);

    MOCKABLE_VIRTUAL size_t estimateFrontEndCmdSizeForMultipleCommandLists(bool isFrontEndStateDirty, uint32_t numCommandLists,
                                                                           ze_command_list_handle_t *phCommandLists);
    size_t estimateFrontEndCmdSize();
    size_t estimatePipelineSelect();
    void programPipelineSelect(NEO::LinearStream &commandStream);

    MOCKABLE_VIRTUAL void handleScratchSpace(NEO::HeapContainer &heapContainer,
                                             NEO::ScratchSpaceController *scratchController,
                                             bool &gsbaState, bool &frontEndState,
                                             uint32_t perThreadScratchSpaceSize,
                                             uint32_t perThreadPrivateScratchSize);

    bool getPreemptionCmdProgramming() override;
    void patchCommands(CommandList &commandList, uint64_t scratchAddress);
};

} // namespace L0

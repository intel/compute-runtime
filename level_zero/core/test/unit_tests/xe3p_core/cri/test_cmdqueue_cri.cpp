/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandQueueHeaplessCri = Test<ModuleFixture>;

CRITEST_F(CommandQueueHeaplessCri, givenHeaplessStateInitWhenEstimateStreamSizeForExecuteCommandListsRegularHeaplessThenEstimatedSizeIsCorrect) {

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    std::unique_ptr<L0::CommandList> commandList2(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList2->close();
    auto cmdListHandle2 = commandList2.get()->toHandle();

    ze_command_list_handle_t cmdListHandles[] = {cmdListHandle, cmdListHandle2};

    auto ctx = typename MockCommandQueueHw<FamilyType::gfxCoreFamily>::CommandListExecutionContext{&cmdListHandle,
                                                                                                   1,
                                                                                                   csr->getPreemptionMode(),
                                                                                                   device,
                                                                                                   csr->getScratchSpaceController(),
                                                                                                   csr->getGlobalStatelessHeapAllocation(),
                                                                                                   false,
                                                                                                   csr->isProgramActivePartitionConfigRequired(),
                                                                                                   false,
                                                                                                   false};
    ctx.globalInit = false;

    {
        ctx.isDirectSubmissionEnabled = false;
        ctx.isDispatchTaskCountPostSyncRequired = false;
        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 1, &cmdListHandle, false, false);

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_END);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
    {
        ctx.isDirectSubmissionEnabled = false;
        ctx.isDispatchTaskCountPostSyncRequired = false;
        bool isInstructionCacheFlushRequired = true;
        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 1, &cmdListHandle, isInstructionCacheFlushRequired, false);

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_END);
        expectedSize += sizeof(PIPE_CONTROL);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
    {
        ctx.isDirectSubmissionEnabled = false;
        ctx.isDispatchTaskCountPostSyncRequired = false;
        bool isStateCacheFlushRequired = true;
        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 1, &cmdListHandle, false, isStateCacheFlushRequired);

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_END);
        expectedSize += sizeof(PIPE_CONTROL);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
    {
        ctx.isDirectSubmissionEnabled = true;
        ctx.isDispatchTaskCountPostSyncRequired = false;
        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 1, &cmdListHandle, false, false);

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_START);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
    {
        ctx.isDirectSubmissionEnabled = true;
        ctx.isDispatchTaskCountPostSyncRequired = false;
        debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 1, &cmdListHandle, false, false);

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_START);
        expectedSize += sizeof(MI_LOAD_REGISTER_REG);
        expectedSize += sizeof(MI_LOAD_REGISTER_REG);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
    {
        ctx.isDirectSubmissionEnabled = false;
        ctx.isDispatchTaskCountPostSyncRequired = true;
        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 1, &cmdListHandle, false, false);

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_END);
        expectedSize += sizeof(PIPE_CONTROL);
        expectedSize += sizeof(MI_MEM_FENCE);

        EXPECT_EQ(expectedSize, estimatedSize);
    }
    {
        ctx.globalInit = true;
        ctx.isDirectSubmissionEnabled = false;
        ctx.isDispatchTaskCountPostSyncRequired = false;

        size_t expectedSize = 0;
        expectedSize += sizeof(MI_BATCH_BUFFER_END);
        expectedSize += sizeof(MI_BATCH_BUFFER_START);

        auto estimatedSize = commandQueue->estimateStreamSizeForExecuteCommandListsRegularHeapless(ctx, 2, cmdListHandles, false, false);
        EXPECT_EQ(expectedSize, estimatedSize);
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0

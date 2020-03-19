/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/test/unit_test/direct_submission/dispatchers/dispatcher_fixture.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "test.h"

using RenderDispatcheTest = Test<DispatcherFixture>;

using namespace NEO;

HWTEST_F(RenderDispatcheTest, givenRenderWhenAskingForPreemptionCmdSizeThenReturnSetMmioCmdSize) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t expectedCmdSize = 0u;
    if (GetPreemptionTestHwDetails<FamilyType>().supportsPreemptionProgramming()) {
        expectedCmdSize = sizeof(MI_LOAD_REGISTER_IMM);
    }
    RenderDispatcher<FamilyType> renderDispatcher;
    EXPECT_EQ(expectedCmdSize, renderDispatcher.getSizePreemption());
}

HWTEST_F(RenderDispatcheTest, givenRenderWhenAddingPreemptionCmdThenExpectProperMmioAddress) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto preemptionDetails = GetPreemptionTestHwDetails<FamilyType>();

    RenderDispatcher<FamilyType> renderDispatcher;
    renderDispatcher.dispatchPreemption(cmdBuffer);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    MI_LOAD_REGISTER_IMM *immCmd = hwParse.getCommand<MI_LOAD_REGISTER_IMM>();
    if (preemptionDetails.supportsPreemptionProgramming()) {
        ASSERT_NE(nullptr, immCmd);
        uint32_t expectedMmio = preemptionDetails.regAddress;
        uint32_t expectedValue = preemptionDetails.modeToRegValueMap[PreemptionMode::MidBatch];

        EXPECT_EQ(expectedMmio, immCmd->getRegisterOffset());
        EXPECT_EQ(expectedValue, immCmd->getDataDword());

    } else {
        EXPECT_EQ(nullptr, immCmd);
    }
}

HWTEST_F(RenderDispatcheTest, givenRenderWhenAskingForMonitorFenceCmdSizeThenReturnRequiredPipeControlCmdSize) {
    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(hardwareInfo);

    RenderDispatcher<FamilyType> renderDispatcher;
    EXPECT_EQ(expectedSize, renderDispatcher.getSizeMonitorFence(hardwareInfo));
}

HWTEST_F(RenderDispatcheTest, givenRenderWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xFF00FF0000ull;
    uint64_t value = 0x102030;
    uint32_t gpuVaLow = static_cast<uint32_t>(gpuVa & 0x0000FFFFFFFFull);
    uint32_t gpuVaHigh = static_cast<uint32_t>(gpuVa >> 32);

    RenderDispatcher<FamilyType> renderDispatcher;
    renderDispatcher.dispatchMonitorFence(cmdBuffer, gpuVa, value, hardwareInfo);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);

    bool foundMonitorFence = false;
    for (auto &it : hwParse.cmdList) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(it);
        if (pipeControl) {
            foundMonitorFence =
                (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) &&
                (pipeControl->getAddress() == gpuVaLow) &&
                (pipeControl->getAddressHigh() == gpuVaHigh) &&
                (pipeControl->getImmediateData() == value);
            if (foundMonitorFence) {
                break;
            }
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

HWTEST_F(RenderDispatcheTest, givenRenderWhenAskingForCacheFlushCmdSizeThenReturnSetRequiredPipeControls) {
    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForFullCacheFlush();

    RenderDispatcher<FamilyType> renderDispatcher;
    size_t actualSize = renderDispatcher.getSizeCacheFlush(hardwareInfo);
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(RenderDispatcheTest, givenRenderWhenAddingCacheFlushCmdThenExpectPipeControlWithProperFields) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    RenderDispatcher<FamilyType> renderDispatcher;
    renderDispatcher.dispatchCacheFlush(cmdBuffer, hardwareInfo);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);

    bool foundCacheFlush = false;
    for (auto &it : hwParse.cmdList) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(it);
        if (pipeControl) {
            foundCacheFlush =
                pipeControl->getRenderTargetCacheFlushEnable() &&
                pipeControl->getInstructionCacheInvalidateEnable() &&
                pipeControl->getTextureCacheInvalidationEnable() &&
                pipeControl->getPipeControlFlushEnable() &&
                pipeControl->getStateCacheInvalidationEnable();
            if (foundCacheFlush) {
                break;
            }
        }
    }
    EXPECT_TRUE(foundCacheFlush);
}

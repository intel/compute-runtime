/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/direct_submission/dispatchers/dispatcher_fixture.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

using RenderDispatcherTest = Test<DispatcherFixture>;

using namespace NEO;

HWTEST_F(RenderDispatcherTest, givenRenderWhenAskingForPreemptionCmdSizeThenReturnSetMmioCmdSize) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t expectedCmdSize = 0u;
    if (getPreemptionTestHwDetails<FamilyType>().supportsPreemptionProgramming()) {
        expectedCmdSize = sizeof(MI_LOAD_REGISTER_IMM);
    }
    EXPECT_EQ(expectedCmdSize, RenderDispatcher<FamilyType>::getSizePreemption());
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenAddingPreemptionCmdThenExpectProperMmioAddress) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto preemptionDetails = getPreemptionTestHwDetails<FamilyType>();

    RenderDispatcher<FamilyType>::dispatchPreemption(cmdBuffer);

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

HWTEST_F(RenderDispatcherTest, givenRenderWhenAskingForMonitorFenceCmdSizeThenReturnRequiredPipeControlCmdSize) {
    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hardwareInfo, false);

    EXPECT_EQ(expectedSize, RenderDispatcher<FamilyType>::getSizeMonitorFence(hardwareInfo));
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xFF00FF0000ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuVa, value, hardwareInfo, false, false, false);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);

    bool foundMonitorFence = false;
    for (auto &it : hwParse.cmdList) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(it);
        if (pipeControl) {
            foundMonitorFence =
                (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) &&
                (NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl) == gpuVa) &&
                (pipeControl->getImmediateData() == value);
            if (foundMonitorFence) {
                break;
            }
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenAskingForCacheFlushCmdSizeThenReturnSetRequiredPipeControls) {
    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForFullCacheFlush();

    size_t actualSize = RenderDispatcher<FamilyType>::getSizeCacheFlush(hardwareInfo);
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenAddingCacheFlushCmdThenExpectPipeControlWithProperFields) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    RenderDispatcher<FamilyType>::dispatchCacheFlush(cmdBuffer, hardwareInfo, 0ull);

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

HWCMDTEST_F(IGFX_XE_HP_CORE, RenderDispatcherTest,
            givenRenderDispatcherPartitionedWorkloadFlagTrueWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValueAndPartitionParameter) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xBADA550000ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuVa, value, hardwareInfo, false, true, false);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundMonitorFence = false;
    for (auto &it : hwParse.pipeControlList) {
        PIPE_CONTROL *pipeControl = reinterpret_cast<PIPE_CONTROL *>(it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundMonitorFence = true;
            EXPECT_EQ(gpuVa, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            EXPECT_EQ(value, pipeControl->getImmediateData());
            EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
            break;
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenCheckingForMultiTileSynchronizationSupportThenExpectTrue) {
    EXPECT_TRUE(RenderDispatcher<FamilyType>::isMultiTileSynchronizationSupported());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, RenderDispatcherTest,
            givenRenderDispatcherNotifyFlagTrueWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValueAndNotifyParameter) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuAddress = 0xADD35500ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuAddress, value, hardwareInfo, true, false, false);

    HardwareParse hwParse;
    hwParse.parsePipeControl = true;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    hwParse.findHardwareCommands<FamilyType>();

    bool foundMonitorFence = false;
    for (auto &it : hwParse.pipeControlList) {
        PIPE_CONTROL *pipeControl = reinterpret_cast<PIPE_CONTROL *>(it);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            foundMonitorFence = true;
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            EXPECT_EQ(value, pipeControl->getImmediateData());
            EXPECT_TRUE(pipeControl->getNotifyEnable());
            break;
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

HWTEST_F(RenderDispatcherTest, givenRenderWithDcFlushFlagTrueWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValueAndDcFlushParameter) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xFF00FF0000ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuVa, value, hardwareInfo, false, false, true);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);

    bool foundMonitorFence = false;
    for (auto &it : hwParse.cmdList) {
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(it);
        if (pipeControl) {
            foundMonitorFence =
                (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) &&
                (NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl) == gpuVa) &&
                (pipeControl->getImmediateData() == value);
            if (foundMonitorFence) {
                EXPECT_TRUE(pipeControl->getDcFlushEnable());
                break;
            }
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

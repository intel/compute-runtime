/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
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
    size_t expectedSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(this->pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);

    EXPECT_EQ(expectedSize, RenderDispatcher<FamilyType>::getSizeMonitorFence(this->pDevice->getRootDeviceEnvironment()));
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xFF00FF0000ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuVa, value, this->pDevice->getRootDeviceEnvironment(), false, false, false);

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

HWCMDTEST_F(IGFX_XE_HP_CORE, RenderDispatcherTest,
            givenRenderDispatcherPartitionedWorkloadFlagTrueWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValueAndPartitionParameter) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xBADA550000ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuVa, value, this->pDevice->getRootDeviceEnvironment(), true, false, true);

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
            EXPECT_FALSE(pipeControl->getTlbInvalidate());
            EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
            EXPECT_TRUE(pipeControl->getNotifyEnable());
            break;
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenCheckingForMultiTileSynchronizationSupportThenExpectTrue) {
    EXPECT_TRUE(RenderDispatcher<FamilyType>::isMultiTileSynchronizationSupported());
}

HWTEST_F(RenderDispatcherTest, givenRenderWithDcFlushFlagTrueWhenAddingMonitorFenceCmdThenExpectPipeControlWithProperAddressAndValueAndDcFlushParameter) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t gpuVa = 0xFF00FF0000ull;
    uint64_t value = 0x102030;

    RenderDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, gpuVa, value, this->pDevice->getRootDeviceEnvironment(), false, true, false);

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
                EXPECT_FALSE(pipeControl->getTlbInvalidate());
                EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
                break;
            }
        }
    }
    EXPECT_TRUE(foundMonitorFence);
}

HWTEST_F(RenderDispatcherTest, givenRenderWhenAskingIsCopyThenReturnFalse) {
    EXPECT_FALSE(RenderDispatcher<FamilyType>::isCopy());
}

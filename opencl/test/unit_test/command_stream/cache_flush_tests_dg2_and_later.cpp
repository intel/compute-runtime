/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/l3_range.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/cmd_buffer_validator.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"

using namespace NEO;

using CacheFlushTestsDg2AndLater = HardwareCommandsTest;

HWTEST2_F(CacheFlushTestsDg2AndLater, WhenProgrammingCacheFlushAfterWalkerThenExpectProperCacheFlushCommand, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &commandStream = cmdQ.getCS(1024);

    void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
    MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
    this->mockKernelWithInternal->mockProgram->setGlobalSurface(&globalAllocation);

    constexpr uint64_t postSyncAddress = 1024;

    HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, postSyncAddress);

    std::string err;
    std::vector<MatchCmd *> expectedCommands;

    expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(
        1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getUnTypedDataPortCacheFlush, true)}));

    if constexpr (FamilyType::isUsingL3Control) {
        using L3_CONTROL = typename FamilyType::L3_CONTROL;
        expectedCommands.push_back(new MatchHwCmd<FamilyType, L3_CONTROL>(
            1, Expects{EXPECT_MEMBER(L3_CONTROL, getUnTypedDataPortCacheFlush, false)}));
    }

    bool cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0, std::move(expectedCommands), &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    this->mockKernelWithInternal->mockProgram->setGlobalSurface(nullptr);
}

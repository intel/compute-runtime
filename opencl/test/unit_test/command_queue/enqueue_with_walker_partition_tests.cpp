/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

struct EnqueueWithWalkerPartitionTests : public ::testing::Test {
    void SetUp() override {
        if (!OSInterface::osEnableLocalMemory) {
            GTEST_SKIP();
        }
        DebugManager.flags.EnableWalkerPartition.set(1u);
        DebugManager.flags.CreateMultipleSubDevices.set(numberOfTiles);

        rootDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, 0));

        context = std::make_unique<MockContext>(rootDevice.get());

        engineControlForFusedQueue = rootDevice->getDefaultEngine();
    }

    DebugManagerStateRestore restore;
    VariableBackup<bool> mockDeviceFlagBackup{&MockDevice::createSingleDevice, false};
    const uint32_t numberOfTiles = 3;
    EngineControl engineControlForFusedQueue = {};
    std::unique_ptr<MockClDevice> rootDevice;
    std::unique_ptr<MockContext> context;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, EnqueueWithWalkerPartitionTests,
            givenCsrWithSpecificNumberOfTilesAndPipeControlWithStallRequiredWhenDispatchingThenConstructCmdBufferForAllSupportedTiles) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);

    MockCommandQueueHw<FamilyType> commandQueue(context.get(), rootDevice.get(), nullptr);
    commandQueue.gpgpuEngine = &engineControlForFusedQueue;
    rootDevice->setPreemptionMode(PreemptionMode::Disabled);
    MockKernelWithInternals kernel(*rootDevice, context.get());

    size_t offset[3] = {0, 0, 0};
    size_t gws[3] = {32, 32, 32};
    commandQueue.enqueueKernel(kernel, 3, offset, gws, nullptr, 0, nullptr, nullptr);
    auto &cmdStream = commandQueue.getCS(0);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    bool lastSemaphoreFound = false;
    for (auto it = hwParser.cmdList.rbegin(); it != hwParser.cmdList.rend(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);

        if (semaphoreCmd) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreCmd)) {
                continue;
            }
            EXPECT_EQ(numberOfTiles, semaphoreCmd->getSemaphoreDataDword());
            lastSemaphoreFound = true;
            break;
        }
    }
    EXPECT_TRUE(lastSemaphoreFound);
}

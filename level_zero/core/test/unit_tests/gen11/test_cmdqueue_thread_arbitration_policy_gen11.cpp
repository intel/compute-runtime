/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

struct CommandQueueThreadArbitrationPolicyTests : public ::testing::Test {
    void SetUp() override {

        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        auto executionEnvironment = new NEO::MockExecutionEnvironment();
        auto mockBuiltIns = new MockBuiltins();
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        auto driverHandleUlt = whiteboxCast(DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue));
        driverHandle.reset(driverHandleUlt);

        ASSERT_NE(nullptr, driverHandle);

        ze_device_handle_t hDevice;
        uint32_t count = 1;
        ze_result_t result = driverHandle->getDevice(&count, &hDevice);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        device = L0::Device::fromHandle(hDevice);
        ASSERT_NE(nullptr, device);

        ze_command_queue_desc_t queueDesc = {};
        commandQueue = whiteboxCast(CommandQueue::create(productFamily, device,
                                                         neoDevice->getDefaultEngine().commandStreamReceiver,
                                                         &queueDesc,
                                                         false,
                                                         false,
                                                         returnValue));
        ASSERT_NE(nullptr, commandQueue);

        commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue);
        ASSERT_NE(nullptr, commandList);
    }
    void TearDown() override {
        commandList->destroy();
        commandQueue->destroy();
        L0::GlobalDriver = nullptr;
    }

    DebugManagerStateRestore restorer;
    WhiteBox<L0::CommandQueue> *commandQueue = nullptr;
    L0::CommandList *commandList = nullptr;
    std::unique_ptr<L0::ult::WhiteBox<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device;
};

HWTEST2_F(CommandQueueThreadArbitrationPolicyTests,
          whenCommandListIsExecutedThenDefaultRoundRobinThreadArbitrationPolicyIsUsed,
          IsGen11HP) {
    size_t usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                      cmd->getDataDword());
        }
    }
}

HWTEST2_F(CommandQueueThreadArbitrationPolicyTests,
          whenCommandListIsExecutedAndOverrideThreadArbitrationPolicyDebugFlagIsSetToZeroThenAgeBasedThreadArbitrationPolicyIsUsed,
          IsGen11HP) {
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(0);

    size_t usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::AgeBased),
                      cmd->getDataDword());
        }
    }
}

HWTEST2_F(CommandQueueThreadArbitrationPolicyTests,
          whenCommandListIsExecutedAndOverrideThreadArbitrationPolicyDebugFlagIsSetToOneThenRoundRobinThreadArbitrationPolicyIsUsed,
          IsGen11HP) {
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(1);

    size_t usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                      cmd->getDataDword());
        }
    }
}

} // namespace ult
} // namespace L0

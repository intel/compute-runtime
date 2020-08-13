/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

struct CommandQueueThreadArbitrationPolicyTests : public ::testing::Test {
    void SetUp() override {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        auto driverHandleUlt = whitebox_cast(DriverHandle::create(std::move(devices), L0EnvVariables{}));
        driverHandle.reset(driverHandleUlt);

        ASSERT_NE(nullptr, driverHandle);

        ze_device_handle_t hDevice;
        uint32_t count = 1;
        ze_result_t result = driverHandle->getDevice(&count, &hDevice);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        device = L0::Device::fromHandle(hDevice);
        ASSERT_NE(nullptr, device);

        ze_command_queue_desc_t queueDesc = {};
        commandQueue = whitebox_cast(CommandQueue::create(productFamily, device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &queueDesc,
                                                          false));
        ASSERT_NE(nullptr, commandQueue->commandStream);

        commandList = CommandList::create(productFamily, device, false);
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
          IsGen9) {
    size_t usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));
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
          IsGen9) {
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(0);

    size_t usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));
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
          IsGen9) {
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(1);

    size_t usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));
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

struct CommandQueueGroupMultiDevice : public MultiDeviceFixture, public ::testing::Test {
    void SetUp() override {
        MultiDeviceFixture::SetUp();
        uint32_t count = 1;
        ze_device_handle_t hDevice;
        ze_result_t res = driverHandle->getDevice(&count, &hDevice);
        ASSERT_EQ(ZE_RESULT_SUCCESS, res);
        device = L0::Device::fromHandle(hDevice);
        ASSERT_NE(nullptr, device);
    }
    void TearDown() override {
        MultiDeviceFixture::TearDown();
    }
    L0::Device *device = nullptr;
};

HWTEST2_F(CommandQueueGroupMultiDevice,
          givenCommandQueuePropertiesCallThenCallSucceedsAndCommandListImmediateIsCreated, IsGen9) {
    uint32_t count = 0;
    ze_result_t res = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);

    std::vector<ze_command_queue_group_properties_t> queueProperties(count);
    res = device->getCommandQueueGroupProperties(&count, queueProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t queueGroupOrdinal = 0u;
    uint32_t queueGroupIndex = 0u;
    ze_command_queue_desc_t desc = {};
    desc.ordinal = queueGroupOrdinal;
    desc.index = queueGroupIndex;

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               false));
    L0::CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    L0::DeviceImp *deviceImp = reinterpret_cast<L0::DeviceImp *>(device);
    auto expectedCSR = deviceImp->neoDevice->getDeviceById(0)->getEngineGroups()[queueGroupOrdinal][queueGroupIndex].commandStreamReceiver;
    EXPECT_EQ(cmdQueue->getCsr(), expectedCSR);
}

} // namespace ult
} // namespace L0
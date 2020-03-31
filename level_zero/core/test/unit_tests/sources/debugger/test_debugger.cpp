/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen_common/reg_configs/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/mocks/mock_os_library.h"

#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"
#include "test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include <level_zero/ze_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

struct ActiveDebuggerFixture {
    void SetUp() {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

        debugger = new MockActiveSourceLevelDebugger(new MockOsLibrary);
        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(debugger);
        executionEnvironment->initializeMemoryManager();

        device = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
        device->setDebuggerActive(true);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(device));

        auto driverHandleUlt = whitebox_cast(DriverHandle::create(std::move(devices)));
        driverHandle.reset(driverHandleUlt);

        ASSERT_NE(nullptr, driverHandle);

        ze_device_handle_t hDevice;
        uint32_t count = 1;
        ze_result_t result = driverHandle->getDevice(&count, &hDevice);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        deviceL0 = L0::Device::fromHandle(hDevice);
        ASSERT_NE(nullptr, deviceL0);
    }
    void TearDown() {
    }

    std::unique_ptr<L0::ult::WhiteBox<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *device = nullptr;
    L0::Device *deviceL0;
    MockActiveSourceLevelDebugger *debugger = nullptr;
};

using CommandQueueDebugCommandsTest = Test<ActiveDebuggerFixture>;

HWTEST_F(CommandQueueDebugCommandsTest, givenDebuggingEnabledWhenCommandListIsExecutedThenKernelDebugCommandsAreAdded) {
    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, deviceL0, device->getDefaultEngine().commandStreamReceiver, &queueDesc));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, deviceL0)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LE(2u, miLoadImm.size());

    MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[0]);
    ASSERT_NE(nullptr, miLoad);

    EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::registerOffset, miLoad->getRegisterOffset());
    EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());

    miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[1]);
    ASSERT_NE(nullptr, miLoad);

    EXPECT_EQ(TdDebugControlRegisterOffset::registerOffset, miLoad->getRegisterOffset());
    EXPECT_EQ(TdDebugControlRegisterOffset::debugEnabledValue, miLoad->getDataDword());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

using DeviceWithDebuggerEnabledTest = Test<ActiveDebuggerFixture>;

TEST_F(DeviceWithDebuggerEnabledTest, givenDebuggingEnabledWhenDeviceIsCreatedThenItHasDebugSurfaceCreated) {
    EXPECT_NE(nullptr, deviceL0->getDebugSurface());
}

} // namespace ult
} // namespace L0

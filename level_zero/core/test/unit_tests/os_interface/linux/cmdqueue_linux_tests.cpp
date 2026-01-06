/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

struct CommandQueueLinuxTests : public Test<DeviceFixture> {

    void SetUp() override {
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        ultHwConfig.useHwCsr = true;
        ultHwConfig.forceOsAgnosticMemoryManager = false;
        auto *executionEnvironment = new NEO::ExecutionEnvironment();
        prepareDeviceEnvironments(*executionEnvironment);
        executionEnvironment->initializeMemoryManager();
        setupWithExecutionEnvironment(*executionEnvironment);
    }
};

HWTEST2_F(CommandQueueLinuxTests, givenExecBufferErrorOnXeHpcWhenExecutingCommandListsThenOutOfHostMemoryIsReturned, IsXeHpcCore) {
    auto drm = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<DrmMock>();

    drm->execBufferResult = -1;
    drm->baseErrno = false;
    drm->errnoRetVal = EWOULDBLOCK;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);

    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.immutableData.isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::kernelIsa, neoDevice->getDeviceBitfield()}));
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    commandList->close();

    ze_command_list_handle_t cmdListHandles[1] = {commandList->toHandle()};

    returnValue = commandQueue->executeCommandLists(1, cmdListHandles, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, returnValue);
    commandQueue->destroy();
    neoDevice->getMemoryManager()->freeGraphicsMemory(kernel.immutableData.isaGraphicsAllocation.release());
}
} // namespace ult
} // namespace L0

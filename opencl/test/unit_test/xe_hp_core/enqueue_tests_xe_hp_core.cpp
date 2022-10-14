/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct EnqueueMultiTileFixture {
    void setUp() {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        DebugManager.flags.EnableImplicitScaling.set(1);

        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&NEO::OSInterface::osEnableLocalMemory, true);
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&NEO::ImplicitScaling::apiSupport, true);

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), mockRootDeviceIndex));
        context = std::make_unique<MockContext>(clDevice.get());

        mockKernel = std::make_unique<MockKernelWithInternals>(*clDevice, context.get());
        mockKernel->kernelInfo.createKernelAllocation(clDevice->getDevice(), false);

        dispatchInfo = {clDevice.get(), mockKernel->mockKernel, 1, 0, 0, 0};
    }

    void tearDown() {
        clDevice->getMemoryManager()->freeGraphicsMemory(mockKernel->kernelInfo.getGraphicsAllocation());
    }

    template <typename FamilyType>
    std::unique_ptr<MockCommandQueueHw<FamilyType>> createCommandQueue() {
        return std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), clDevice.get(), nullptr);
    }

    DispatchInfo dispatchInfo;
    DebugManagerStateRestore restore;
    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockKernelWithInternals> mockKernel;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
};

using EnqueueMultiTileXeHpCoreTest = Test<EnqueueMultiTileFixture>;

XE_HP_CORE_TEST_F(EnqueueMultiTileXeHpCoreTest, GivenMultiTileWhenDispatchingWalkerThenSynchronizationAfterWalkerHasProperFlag) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    mockKernel->kernelInfo.heapInfo.KernelHeapSize = 1;

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    size_t offset[3] = {0, 0, 0};
    size_t gws[3] = {32, 1, 1};
    commandQueue->enqueueKernel(mockKernel->mockKernel, 3, offset, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream, 0);

    auto itorWalker = find<COMPUTE_WALKER *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), itorWalker);

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorWalker);
    ASSERT_NE(nullptr, walkerCmd);
    EXPECT_EQ(FamilyType::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_X, walkerCmd->getPartitionType());

    auto itorPipeControl = find<PIPE_CONTROL *>(itorWalker, hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), itorPipeControl);

    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*itorPipeControl);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
}

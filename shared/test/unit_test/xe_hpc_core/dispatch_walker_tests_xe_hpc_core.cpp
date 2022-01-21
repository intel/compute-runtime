/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using WalkerDispatchTestsXeHpcCore = ::testing::Test;

XE_HPC_CORETEST_F(WalkerDispatchTestsXeHpcCore, whenEncodeAdditionalWalkerFieldsThenPostSyncDataIsCorrectlySet) {
    struct {
        unsigned short revisionId;
        int32_t programGlobalFenceAsPostSyncOperationInComputeWalker;
        bool expectSystemMemoryFenceRequest;
    } testInputs[] = {
        {0x0, -1, false},
        {0x3, -1, true},
        {0x0, 0, false},
        {0x3, 0, false},
        {0x0, 1, true},
        {0x3, 1, true},
    };

    DebugManagerStateRestore debugRestorer;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto &postSyncData = walkerCmd.getPostSync();
    auto hwInfo = *defaultHwInfo;

    for (auto &testInput : testInputs) {
        hwInfo.platform.usRevId = testInput.revisionId;
        DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(
            testInput.programGlobalFenceAsPostSyncOperationInComputeWalker);

        postSyncData.setSystemMemoryFenceRequest(false);

        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, KernelExecutionType::Default);
        EXPECT_EQ(testInput.expectSystemMemoryFenceRequest, postSyncData.getSystemMemoryFenceRequest());
    }
}

XE_HPC_CORETEST_F(WalkerDispatchTestsXeHpcCore, givenPvcWhenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    DebugManagerStateRestore debugRestorer;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto hwInfo = *defaultHwInfo;

    {
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, KernelExecutionType::Default);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        DebugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, KernelExecutionType::Default);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

XE_HPC_CORETEST_F(WalkerDispatchTestsXeHpcCore, givenPvcXtTemporaryWhenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = 0x0BE5;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;

    {
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, KernelExecutionType::Default);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, KernelExecutionType::Concurrent);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

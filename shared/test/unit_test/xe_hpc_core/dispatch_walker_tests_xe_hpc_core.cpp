/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using WalkerDispatchTestsXeHpcCore = ::testing::Test;

XE_HPC_CORETEST_F(WalkerDispatchTestsXeHpcCore, givenXeHpcWhenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    DebugManagerStateRestore debugRestorer;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs{KernelExecutionType::Default, true, kernelDescriptor};
    {
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

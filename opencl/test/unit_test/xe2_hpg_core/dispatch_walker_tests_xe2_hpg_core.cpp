/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using WalkerDispatchTestsXe2HpGCore = ::testing::Test;

XE2_HPG_CORETEST_F(WalkerDispatchTestsXe2HpGCore, whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    DebugManagerStateRestore debugRestorer;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs{KernelExecutionType::concurrent, true, kernelDescriptor, NEO::RequiredDispatchWalkOrder::none, 0, 0};
    {
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerArgs.kernelExecutionType = KernelExecutionType::defaultType;
        debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);

        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

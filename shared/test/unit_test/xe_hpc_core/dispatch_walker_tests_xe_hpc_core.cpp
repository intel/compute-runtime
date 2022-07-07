/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;

using WalkerDispatchTestsXeHpcCore = ::testing::Test;

XE_HPC_CORETEST_F(WalkerDispatchTestsXeHpcCore, givenXeHpcWhenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    DebugManagerStateRestore debugRestorer;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto hwInfo = *defaultHwInfo;

    EncodeWalkerArgs walkerArgs{KernelExecutionType::Default, true};
    {
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
        uint32_t expectedValue = hwInfoConfig.isComputeDispatchAllWalkerEnableInComputeWalkerRequired(hwInfo);
        walkerArgs.kernelExecutionType = KernelExecutionType::Concurrent;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);
        EXPECT_EQ(expectedValue, walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        DebugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

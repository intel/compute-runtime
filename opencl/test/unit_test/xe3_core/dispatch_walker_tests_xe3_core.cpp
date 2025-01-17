/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using WalkerDispatchTestsXe3Core = ::testing::Test;

XE3_CORETEST_F(WalkerDispatchTestsXe3Core, whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    DebugManagerStateRestore debugRestorer;
    MockExecutionEnvironment executionEnvironment;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs{
        kernelDescriptor,                     // kernelDescriptor
        KernelExecutionType::concurrent,      // kernelExecutionType
        NEO::RequiredDispatchWalkOrder::none, // requiredDispatchWalkOrder
        0,                                    // localRegionSize
        113,                                  // maxFrontEndThreads
        true};                                // requiredSystemFence
    {
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    auto backupCcsNumber = executionEnvironment.rootDeviceEnvironments[0]->getNonLimitedNumberOfCcs();
    executionEnvironment.rootDeviceEnvironments[0]->setNonLimitedNumberOfCcs(1);
    VariableBackup<uint32_t> sliceCountBackup(&executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.SliceCount, 4);

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(FamilyType::INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerArgs.kernelExecutionType = KernelExecutionType::defaultType;
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        VariableBackup<uint32_t> sliceCountBackup(&executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.SliceCount, 2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        EncodeDispatchKernel<FamilyType>::template encodeComputeDispatchAllWalker<typename FamilyType::COMPUTE_WALKER, typename FamilyType::INTERFACE_DESCRIPTOR_DATA>(walkerCmd, nullptr, *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        VariableBackup<uint32_t> maxFrontEndThreadsBackup(&walkerArgs.maxFrontEndThreads, 0u);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(FamilyType::INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(FamilyType::INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
        executionEnvironment.rootDeviceEnvironments[0]->setNonLimitedNumberOfCcs(2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        executionEnvironment.rootDeviceEnvironments[0]->setNonLimitedNumberOfCcs(backupCcsNumber);
        debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

XE3_CORETEST_F(WalkerDispatchTestsXe3Core, givenHasSampleSetWhenEncodingExtraParamsThenSetCorrectFields) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using DISPATCH_WALK_ORDER = typename COMPUTE_WALKER::DISPATCH_WALK_ORDER;
    using THREAD_GROUP_BATCH_SIZE = typename COMPUTE_WALKER::THREAD_GROUP_BATCH_SIZE;

    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs{
        kernelDescriptor,                     // kernelDescriptor
        KernelExecutionType::defaultType,     // kernelExecutionType
        NEO::RequiredDispatchWalkOrder::none, // requiredDispatchWalkOrder
        0,                                    // localRegionSize
        0,                                    // maxFrontEndThreads
        false};                               // requiredSystemFence

    {
        kernelDescriptor.kernelAttributes.flags.hasSample = false;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_NE(DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK, walkerCmd.getDispatchWalkOrder());
        EXPECT_EQ(THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_1, walkerCmd.getThreadGroupBatchSize());
    }

    {
        kernelDescriptor.kernelAttributes.flags.hasSample = true;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_EQ(DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK, walkerCmd.getDispatchWalkOrder());
        EXPECT_EQ(THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_4, walkerCmd.getThreadGroupBatchSize());
    }
}

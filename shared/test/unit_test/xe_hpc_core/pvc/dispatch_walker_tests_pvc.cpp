/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using WalkerDispatchTestsPvc = ::testing::Test;

PVCTEST_F(WalkerDispatchTestsPvc, givenPvcWhenEncodeAdditionalWalkerFieldsThenPostSyncDataIsCorrectlySet) {
    struct {
        unsigned short revisionId;
        int32_t programGlobalFenceAsPostSyncOperationInComputeWalker;
        bool expectSystemMemoryFenceRequest;
    } testInputs[] = {
        {0x0, -1, true},
        {0x3, -1, true},
        {0x0, 0, false},
        {0x3, 0, false},
        {0x0, 1, true},
        {0x3, 1, true},
    };

    DebugManagerStateRestore debugRestorer;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto &postSyncData = walkerCmd.getPostSync();

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs = CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(kernelDescriptor);
    walkerArgs.requiredSystemFence = true;

    for (auto &testInput : testInputs) {
        for (auto &deviceId : pvcXlDeviceIds) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = testInput.revisionId;
            debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(
                testInput.programGlobalFenceAsPostSyncOperationInComputeWalker);

            postSyncData.setSystemMemoryFenceRequest(false);
            EncodeDispatchKernel<FamilyType>::encodeWalkerPostSyncFields(walkerCmd, rootDeviceEnvironment, walkerArgs);
            EXPECT_EQ(testInput.expectSystemMemoryFenceRequest, postSyncData.getSystemMemoryFenceRequest());
        }
    }
}

PVCTEST_F(WalkerDispatchTestsPvc, givenPvcSupportsSystemMemoryFenceWhenNoSystemFenceRequiredThenEncodedWalkerFenceFieldSetToFalse) {
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto &postSyncData = walkerCmd.getPostSync();

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.platform.usRevId = 0x3;

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs = CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(kernelDescriptor);
    for (auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        postSyncData.setSystemMemoryFenceRequest(true);
        EncodeDispatchKernel<FamilyType>::encodeWalkerPostSyncFields(walkerCmd, rootDeviceEnvironment, walkerArgs);
        EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
    }
}

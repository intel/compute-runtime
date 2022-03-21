/*
 * Copyright (C) 2022 Intel Corporation
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

using WalkerDispatchTestsPvc = ::testing::Test;

PVCTEST_F(WalkerDispatchTestsPvc, givenPvcWhenEncodeAdditionalWalkerFieldsThenPostSyncDataIsCorrectlySet) {
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
        for (auto &deviceId : PVC_XL_IDS) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = testInput.revisionId;
            DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(
                testInput.programGlobalFenceAsPostSyncOperationInComputeWalker);

            postSyncData.setSystemMemoryFenceRequest(false);
            EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, KernelExecutionType::Default);
            EXPECT_EQ(testInput.expectSystemMemoryFenceRequest, postSyncData.getSystemMemoryFenceRequest());
        }
    }
}
/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

using CommandQueueXe3pAndLaterTests = ::testing::Test;

HWTEST2_F(CommandQueueXe3pAndLaterTests, givenCmdQueueWhenPlatformWithStatelessDisableWithDebugKeyThenStatelessIsDisabled, IsWithinXeHpcCoreAndXe3pCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableForceToStateless.set(1);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);

    EXPECT_FALSE(mockCmdQ->isForceStateless);
}

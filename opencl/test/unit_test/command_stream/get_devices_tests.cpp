/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/source/platform/platform.h"

#include "gtest/gtest.h"

namespace NEO {
TEST(MultiDeviceTests, givenCreateMultipleRootDevicesAndLimitAmountOfReturnedDevicesFlagWhenClGetDeviceIdsIsCalledThenLowerValueIsReturned) {
    platformsImpl->clear();
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    ultHwConfig.forceOsAgnosticMemoryManager = false;
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore stateRestore;
    debugManager.flags.CreateMultipleRootDevices.set(2);
    debugManager.flags.LimitAmountOfReturnedDevices.set(1);
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numDevices);
}
} // namespace NEO

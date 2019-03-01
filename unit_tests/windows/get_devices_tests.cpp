/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "test.h"

namespace OCLRT {
bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT

using GetDevicesTests = ::testing::Test;

HWTEST_F(GetDevicesTests, WhenGetDevicesIsCalledThenSuccessIsReturned) {
    OCLRT::HardwareInfo *hwInfo;
    size_t numDevicesReturned = 0;
    OCLRT::ExecutionEnvironment executionEnviornment;

    auto returnValue = OCLRT::getDevices(&hwInfo, numDevicesReturned, executionEnviornment);
    EXPECT_EQ(true, returnValue);
}

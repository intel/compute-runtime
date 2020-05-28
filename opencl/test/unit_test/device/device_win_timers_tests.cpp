/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_ostime.h"
#include "opencl/test/unit_test/mocks/mock_ostime_win.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace ULT {

typedef ::testing::Test MockOSTimeWinTest;

TEST_F(MockOSTimeWinTest, WhenCreatingTimerThenResolutionIsSetCorrectly) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddmMock = new WddmMock(rootDeviceEnvironment);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    wddmMock->init();

    std::unique_ptr<MockOSTimeWin> timeWin(new MockOSTimeWin(wddmMock));

    double res = 0.0;
    res = timeWin->getDynamicDeviceTimerResolution(device->getHardwareInfo());
    EXPECT_EQ(res, 1e+09);
}

} // namespace ULT

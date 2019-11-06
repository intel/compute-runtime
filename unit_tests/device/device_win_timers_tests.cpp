/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/root_device_environment.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/mocks/mock_ostime_win.h"
#include "unit_tests/mocks/mock_wddm.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace ULT {

typedef ::testing::Test MockOSTimeWinTest;

TEST_F(MockOSTimeWinTest, DynamicResolution) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddmMock = std::unique_ptr<WddmMock>(new WddmMock(rootDeviceEnvironment));
    auto mDev = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto hwInfo = mDev->getHardwareInfo();
    wddmMock->init(hwInfo);

    std::unique_ptr<MockOSTimeWin> timeWin(new MockOSTimeWin(wddmMock.get()));

    double res = 0.0;
    res = timeWin->getDynamicDeviceTimerResolution(mDev->getHardwareInfo());
    EXPECT_EQ(res, 1e+09);
}

} // namespace ULT

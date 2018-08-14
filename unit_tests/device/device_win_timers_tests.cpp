/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
#include "gtest/gtest.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_ostime_win.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "test.h"

using namespace OCLRT;

namespace ULT {

typedef ::testing::Test MockOSTimeWinTest;

HWTEST_F(MockOSTimeWinTest, DynamicResolution) {
    auto wddmMock = std::unique_ptr<WddmMock>(new WddmMock());
    auto mDev = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    bool error = wddmMock->init<FamilyType>();
    EXPECT_EQ(1u, wddmMock->createContextResult.called);

    std::unique_ptr<MockOSTimeWin> timeWin(new MockOSTimeWin(wddmMock.get()));

    double res = 0.0;
    res = timeWin->getDynamicDeviceTimerResolution(mDev->getHardwareInfo());
    EXPECT_EQ(res, 1e+09);
}

} // namespace ULT

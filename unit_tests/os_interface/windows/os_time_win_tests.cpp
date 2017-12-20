/*
* Copyright (c) 2017, Intel Corporation
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
#include "mock_os_time_win.h"
#include <memory>

using namespace OCLRT;
struct OSTimeWinTest : public ::testing::Test {
  public:
    virtual void SetUp() override {
        osTime = std::unique_ptr<MockOSTimeWin>(new MockOSTimeWin(nullptr));
    }

    virtual void TearDown() override {
    }
    std::unique_ptr<MockOSTimeWin> osTime;
};

TEST_F(OSTimeWinTest, givenZeroFrequencyWhenGetHostTimerFuncIsCalledThenReturnsZero) {
    LARGE_INTEGER frequency;
    frequency.QuadPart = 0;
    osTime->setFrequency(frequency);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(0, retVal);
}

TEST_F(OSTimeWinTest, givenNonZeroFrequencyWhenGetHostTimerFuncIsCalledThenReturnsNonZero) {
    LARGE_INTEGER frequency;
    frequency.QuadPart = NSEC_PER_SEC;
    osTime->setFrequency(frequency);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(1.0, retVal);
}

TEST_F(OSTimeWinTest, givenOsTimeWinWhenGetCpuRawTimestampIsCalledThenReturnsNonZero) {
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_NE(0ull, retVal);
}

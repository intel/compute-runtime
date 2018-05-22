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

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"
#include "test.h"

using namespace OCLRT;
struct Gen8DeviceTest : public DeviceFixture,
                        public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
};

BDWTEST_F(Gen8DeviceTest, givenGen8DeviceWhenAskedForClVersionThenReport21) {
    auto version = pDevice->getSupportedClVersion();
    EXPECT_EQ(21u, version);
}

BDWTEST_F(Gen8DeviceTest, givenSourceLevelDebuggerAvailableWhenDeviceIsCreatedThenSourceLevelDebuggerIsDisabled) {
    auto device = std::unique_ptr<MockDeviceWithSourceLevelDebugger<MockActiveSourceLevelDebugger>>(Device::create<MockDeviceWithSourceLevelDebugger<MockActiveSourceLevelDebugger>>(nullptr));
    const auto &caps = device->getDeviceInfo();
    EXPECT_NE(nullptr, device->getSourceLevelDebugger());
    EXPECT_FALSE(caps.sourceLevelDebuggerActive);
}

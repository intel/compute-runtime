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

#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/libult/create_command_stream.h"
#include "runtime/device/device.h"
#include "runtime/platform/platform.h"
#include "gtest/gtest.h"

namespace OCLRT {

PlatformFixture::PlatformFixture()
    : pPlatform(nullptr), num_devices(0), devices(nullptr) {
}

void PlatformFixture::SetUp() {
    pPlatform = constructPlatform();
    ASSERT_EQ(0u, pPlatform->getNumDevices());

    // setup platform / context
    bool isInitialized = pPlatform->initialize();
    ASSERT_EQ(true, isInitialized);

    num_devices = static_cast<cl_uint>(pPlatform->getNumDevices());
    ASSERT_GT(num_devices, 0u);

    auto allDev = pPlatform->getDevices();
    ASSERT_NE(nullptr, allDev);

    devices = new cl_device_id[num_devices];
    for (cl_uint deviceOrdinal = 0; deviceOrdinal < num_devices; ++deviceOrdinal) {
        auto device = allDev[deviceOrdinal];
        ASSERT_NE(nullptr, device);
        devices[deviceOrdinal] = device;
    }
}

void PlatformFixture::TearDown() {
    platformImpl.reset(nullptr);
    delete[] devices;
}
} // namespace OCLRT

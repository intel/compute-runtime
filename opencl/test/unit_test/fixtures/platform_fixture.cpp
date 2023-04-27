/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {
void PlatformFixture::setUp() {
    pPlatform = constructPlatform();
    ASSERT_EQ(0u, pPlatform->getNumDevices());

    // setup platform / context
    bool isInitialized = initPlatform();
    ASSERT_EQ(true, isInitialized);

    numDevices = static_cast<cl_uint>(pPlatform->getNumDevices());
    ASSERT_GT(numDevices, 0u);

    auto allDev = pPlatform->getClDevices();
    ASSERT_NE(nullptr, allDev);

    devices = new cl_device_id[numDevices];
    for (cl_uint deviceOrdinal = 0; deviceOrdinal < numDevices; ++deviceOrdinal) {
        auto device = allDev[deviceOrdinal];
        ASSERT_NE(nullptr, device);
        devices[deviceOrdinal] = device;
    }
}

void PlatformFixture::tearDown() {
    platformsImpl->clear();
    delete[] devices;
}
} // namespace NEO

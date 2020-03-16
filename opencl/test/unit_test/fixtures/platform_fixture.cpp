/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

#include "opencl/source/device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {
void PlatformFixture::SetUp() {
    pPlatform = constructPlatform();
    ASSERT_EQ(0u, pPlatform->getNumDevices());

    // setup platform / context
    bool isInitialized = initPlatform();
    ASSERT_EQ(true, isInitialized);

    num_devices = static_cast<cl_uint>(pPlatform->getNumDevices());
    ASSERT_GT(num_devices, 0u);

    auto allDev = pPlatform->getClDevices();
    ASSERT_NE(nullptr, allDev);

    devices = new cl_device_id[num_devices];
    for (cl_uint deviceOrdinal = 0; deviceOrdinal < num_devices; ++deviceOrdinal) {
        auto device = allDev[deviceOrdinal];
        ASSERT_NE(nullptr, device);
        devices[deviceOrdinal] = device;
    }
}

void PlatformFixture::TearDown() {
    platformsImpl.clear();
    delete[] devices;
}
} // namespace NEO

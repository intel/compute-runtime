/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/platform_fixture.h"

#include "runtime/device/device.h"

#include "gtest/gtest.h"

namespace NEO {

PlatformFixture::PlatformFixture()
    : pPlatform(nullptr), num_devices(0), devices(nullptr)

{
}

void PlatformFixture::SetUp(size_t numDevices, const HardwareInfo **pDevices) {
    pPlatform = platform();
    ASSERT_EQ(0u, pPlatform->getNumDevices());

    // setup platform / context
    bool isInitialized = pPlatform->initialize(numDevices, pDevices);
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
    pPlatform->shutdown();
    delete[] devices;
}
} // namespace NEO

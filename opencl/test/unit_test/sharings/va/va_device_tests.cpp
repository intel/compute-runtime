/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/va/va_device.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "test.h"

using namespace NEO;

using VaDeviceTests = Test<PlatformFixture>;

TEST_F(VaDeviceTests, givenVADeviceWhenGetDeviceFromVAIsCalledThenRootDeviceIsReturned) {
    VADisplay vaDisplay = nullptr;

    VADevice vaDevice{};
    ClDevice *device = vaDevice.getDeviceFromVA(pPlatform, vaDisplay);

    EXPECT_EQ(pPlatform->getClDevice(0), device);
}

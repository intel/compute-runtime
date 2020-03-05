/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> KblDeviceCaps;

KBLTEST_F(KblDeviceCaps, reportsOcl21) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
}

KBLTEST_F(KblDeviceCaps, GivenKBLWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

KBLTEST_F(KblDeviceCaps, givenKblWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

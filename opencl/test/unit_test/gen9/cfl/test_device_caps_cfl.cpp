/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> CflDeviceCaps;

CFLTEST_F(CflDeviceCaps, GivenCFLWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

CFLTEST_F(CflDeviceCaps, givenCflWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

using Dg1ClDeviceCaps = Test<ClDeviceFixture>;

DG1TEST_F(Dg1ClDeviceCaps, givenDg1hpWhenInitializeCapsThenVmeIsNotSupported) {
    pClDevice->driverInfo.reset();
    pClDevice->name.clear();
    pClDevice->initializeCaps();

    cl_uint expectedVmeAvcVersion = CL_AVC_ME_VERSION_0_INTEL;
    cl_uint expectedVmeVersion = CL_ME_VERSION_LEGACY_INTEL;

    EXPECT_EQ(expectedVmeVersion, pClDevice->getDeviceInfo().vmeVersion);
    EXPECT_EQ(expectedVmeAvcVersion, pClDevice->getDeviceInfo().vmeAvcVersion);

    EXPECT_FALSE(pClDevice->getDeviceInfo().vmeAvcSupportsTextureSampler);
    EXPECT_FALSE(pDevice->getDeviceInfo().vmeAvcSupportsPreemption);
}

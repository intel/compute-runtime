/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/gen12lp/hw_info_dg1.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"
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

DG1TEST_F(GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledForDg1ThenClSuccessIsReturned) {

    std::vector<TestParams> params = {
        {CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL,
         CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL},
        {CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL, 0},
        {CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL, 0}};

    check(params);
}

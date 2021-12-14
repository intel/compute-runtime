/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using Dg1DeviceCaps = Test<ClDeviceFixture>;

DG1TEST_F(Dg1DeviceCaps, givenDg1WhenCheckSupportCacheFlushAfterWalkerThenFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

DG1TEST_F(Dg1DeviceCaps, givenDG1WhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

DG1TEST_F(Dg1DeviceCaps, givenDG1WhenRequestedVmeFlagsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsVme);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcPreemption);
}

DG1TEST_F(Dg1DeviceCaps, givenDg1hpWhenInitializeCapsThenVmeIsNotSupported) {
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

DG1TEST_F(Dg1DeviceCaps, givenDg1WhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

DG1TEST_F(Dg1DeviceCaps, givenDg1WhenCheckGpuAdressSpaceThenReturn47bits) {
    EXPECT_EQ(MemoryConstants::max64BitAppAddress, pDevice->getHardwareInfo().capabilityTable.gpuAddressSpace);
}

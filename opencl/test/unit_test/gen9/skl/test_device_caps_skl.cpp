/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_skl.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> SklDeviceCaps;

SKLTEST_F(SklDeviceCaps, WhenCheckingProfilingTimerResolutionThenCorrectResolutionIsReturned) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}

SKLTEST_F(SklDeviceCaps, givenSklDeviceWhenAskedFor32BitSupportThenFalseIsReturned) {
    const auto &sharedCaps = pDevice->getDeviceInfo();
    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
    EXPECT_FALSE(sharedCaps.force32BitAddressess);
}

SKLTEST_F(SklDeviceCaps, WhenCheckingCapabilitiesThenSvmIsEnabled) {
    const auto &caps = pClDevice->getDeviceInfo();
    cl_device_svm_capabilities expectedCaps = (CL_DEVICE_SVM_COARSE_GRAIN_BUFFER |
                                               CL_DEVICE_SVM_FINE_GRAIN_BUFFER |
                                               CL_DEVICE_SVM_ATOMICS);
    EXPECT_EQ(expectedCaps, caps.svmCapabilities);
}

SKLTEST_F(SklDeviceCaps, GivenSKLWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

SKLTEST_F(SklDeviceCaps, givenSklWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

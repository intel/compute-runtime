/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_bxt.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

typedef Test<ClDeviceFixture> BxtDeviceCaps;

BXTTEST_F(BxtDeviceCaps, WhenCheckingProfilingTimerResolutionThenCorrectResolutionIsReturned) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(52u, caps.outProfilingTimerResolution);
}

BXTTEST_F(BxtDeviceCaps, givenBxtDeviceWhenAskedFor32BitSupportThenCorrectValuesAreReturned) {
    const auto &sharedCaps = pDevice->getDeviceInfo();
    auto memoryManager = pDevice->getMemoryManager();
    if constexpr (is64bit) {
        EXPECT_TRUE(memoryManager->peekForce32BitAllocations());
        EXPECT_TRUE(sharedCaps.force32BitAddressess);
    } else {
        EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
        EXPECT_FALSE(sharedCaps.force32BitAddressess);
    }
}

BXTTEST_F(BxtDeviceCaps, WhenCheckingCapabilitiesThenSvmIsNotSupported) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.svmCapabilities);
}

BXTTEST_F(BxtDeviceCaps, WhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

BXTTEST_F(BxtDeviceCaps, WhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

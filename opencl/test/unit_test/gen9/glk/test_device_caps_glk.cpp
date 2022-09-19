/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_glk.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using GlkDeviceCaps = Test<DeviceFixture>;

GLKTEST_F(GlkDeviceCaps, WhenCheckingProfilingTimerResolutionThenCorrectResolutionIsReturned) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(52u, caps.outProfilingTimerResolution);
}

GLKTEST_F(GlkDeviceCaps, givenGlkDeviceWhenAskedForDoubleSupportThenTrueIsReturned) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}

GLKTEST_F(GlkDeviceCaps, GlkIs32BitOsAllocatorAvailable) {
    const auto &caps = pDevice->getDeviceInfo();
    auto memoryManager = pDevice->getMemoryManager();
    if constexpr (is64bit) {
        EXPECT_TRUE(memoryManager->peekForce32BitAllocations());
        EXPECT_TRUE(caps.force32BitAddressess);
    } else {
        EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
        EXPECT_FALSE(caps.force32BitAddressess);
    }
}

GLKTEST_F(GlkDeviceCaps, GivenGLKWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

GLKTEST_F(GlkDeviceCaps, givenGlkWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

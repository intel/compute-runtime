/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/gen12lp/hw_info_dg1.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Dg1DeviceCaps = Test<DeviceFixture>;

DG1TEST_F(Dg1DeviceCaps, givenDg1WhenCheckSupportCacheFlushAfterWalkerThenFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

DG1TEST_F(Dg1DeviceCaps, givenDg1WhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

DG1TEST_F(Dg1DeviceCaps, givenDg1WhenCheckGpuAddressSpaceThenReturn47bits) {
    EXPECT_EQ(MemoryConstants::max64BitAppAddress, pDevice->getHardwareInfo().capabilityTable.gpuAddressSpace);
}

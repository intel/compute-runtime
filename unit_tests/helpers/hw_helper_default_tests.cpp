/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "unit_tests/helpers/hw_helper_tests.h"

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo) {
    HardwareCapabilities hwCaps = {0};

    hwHelper.setupHardwareCapabilities(&hwCaps, hwInfo);

    EXPECT_EQ(16384u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(16384u, hwCaps.image3DMaxWidth);
    EXPECT_TRUE(hwCaps.isStatelesToStatefullWithOffsetSupported);
}

/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo) {
    HardwareCapabilities hwCaps = {0};

    hwHelper.setupHardwareCapabilities(&hwCaps, hwInfo);

    EXPECT_EQ(16384u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(16384u, hwCaps.image3DMaxWidth);
    EXPECT_TRUE(hwCaps.isStatelesToStatefullWithOffsetSupported);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenAskedForHvAlign4RequiredThenReturnTrue) {
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.hvAlign4Required());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenAskedForLowPriorityEngineTypeThenReturnRcs) {
    auto hwHelperEngineType = HwHelperHw<FamilyType>::lowPriorityEngineType;
    EXPECT_EQ(aub_stream::EngineType::ENGINE_RCS, hwHelperEngineType);
}

/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/hw_test.h"

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenAskedForHvAlign4RequiredThenReturnTrue) {
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.hvAlign4Required());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenGettingBindlessSurfaceExtendedMessageDescriptorValueThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto value = hwHelper.getBindlessSurfaceExtendedMessageDescriptorValue(0x200);

    typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor messageExtDescriptor = {};
    messageExtDescriptor.setBindlessSurfaceOffset(0x200);

    EXPECT_EQ(messageExtDescriptor.getBindlessSurfaceOffsetToPatch(), value);
    EXPECT_EQ(0x200u << 6, value);
}

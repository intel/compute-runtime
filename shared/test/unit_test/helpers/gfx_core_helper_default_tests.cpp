/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenAskedForHvAlign4RequiredThenReturnTrue) {
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    EXPECT_TRUE(gfxCoreHelper.hvAlign4Required());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenGettingBindlessSurfaceExtendedMessageDescriptorValueThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto value = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(0x200);

    typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor messageExtDescriptor = {};
    messageExtDescriptor.setBindlessSurfaceOffset(0x200);

    EXPECT_EQ(messageExtDescriptor.getBindlessSurfaceOffsetToPatch(), value);
    EXPECT_EQ(0x200u << 6, value);
}

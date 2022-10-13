/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/supported_media_surface_formats.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

extern std::pair<NEO::GFX3DSTATE_SURFACEFORMAT, bool>
    formatsWithSupportValue[171];

using HwHelperMediaFormatsTestGen12LP = ::testing::TestWithParam<std::pair<NEO::GFX3DSTATE_SURFACEFORMAT, bool>>;
INSTANTIATE_TEST_CASE_P(AllFormatsWithMediaBlockSupportValue,
                        HwHelperMediaFormatsTestGen12LP,
                        ::testing::ValuesIn(formatsWithSupportValue));

GEN12LPTEST_P(HwHelperMediaFormatsTestGen12LP, givenSurfaceFormatWhenIsMediaFormatSupportedThenCorrectValueReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(helper.isSurfaceFormatSupportedForMediaBlockOperation(GetParam().first), GetParam().second);
}
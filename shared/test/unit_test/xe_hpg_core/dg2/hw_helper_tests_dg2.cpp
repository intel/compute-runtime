/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

using namespace NEO;

using HwHelperTestDg2 = ::testing::Test;

DG2TEST_F(HwHelperTestDg2, whenGetExtensionsIsCalledThenMatrixMultiplyAccumulateExtensionsAreReturned) {
    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto extensions = hwHelper.getExtensions(*defaultHwInfo);

    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
}

/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/helpers/simd_helper_tests_pvc_and_later.inl"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using TestSimdConfigSet = ::testing::Test;

XE3_CORETEST_F(TestSimdConfigSet, GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedXe3Core) {
    GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedPVCAndLater<typename FamilyType::COMPUTE_WALKER>::testBodyImpl();
}

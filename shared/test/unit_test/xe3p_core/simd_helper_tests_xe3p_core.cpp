/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/unit_test/helpers/simd_helper_tests_pvc_and_later.inl"

#include "per_product_test_definitions.h"

using namespace NEO;

using TestSimdConfigSet = ::testing::Test;

XE3P_CORETEST_F(TestSimdConfigSet, GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedXe3pCore) {
    GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedPVCAndLater<typename FamilyType::COMPUTE_WALKER>::testBodyImpl();
}

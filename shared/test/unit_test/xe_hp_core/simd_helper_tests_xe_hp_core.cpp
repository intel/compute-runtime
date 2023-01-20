/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/simd_helper_tests.inl"

using namespace NEO;

using TestSimdConfigSet = ::testing::Test;

XE_HP_CORE_TEST_F(TestSimdConfigSet, GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedXeHpCore) {
    GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturned<typename FamilyType::COMPUTE_WALKER>::testBodyImpl();
}
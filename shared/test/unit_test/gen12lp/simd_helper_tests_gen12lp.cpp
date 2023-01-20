/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/simd_helper_tests.inl"

using namespace NEO;

using TestSimdConfigSet = ::testing::Test;

GEN12LPTEST_F(TestSimdConfigSet, GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedGen12LP) {
    GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturned<typename FamilyType::GPGPU_WALKER>::testBodyImpl();
}
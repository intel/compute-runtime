/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/simd_helper_tests.inl"

using namespace NEO;

using TestSimdConfigSet = ::testing::Test;

GEN8TEST_F(TestSimdConfigSet, GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedGen8) {
    GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturned<typename FamilyType::GPGPU_WALKER>::TestBodyImpl();
}

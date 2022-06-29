/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/simd_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

template <typename WALKER_TYPE>
class GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedPVCAndLater {
  public:
    static void TestBodyImpl() { // NOLINT(readability-identifier-naming)
        uint32_t simd = 32;
        auto result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMT32);

        simd = 16;
        result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMT16);

        simd = 1;
        result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMT32);
    }
};
} // namespace NEO

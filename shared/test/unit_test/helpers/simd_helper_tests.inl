/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/simd_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

template <typename WALKER_TYPE>
class GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturned {
  public:
    static void TestBodyImpl() { // NOLINT(readability-identifier-naming)
        uint32_t simd = 32;
        auto result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMD32);

        simd = 16;
        result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMD16);

        simd = 8;
        result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMD8);

        simd = 1;
        result = getSimdConfig<WALKER_TYPE>(simd);
        EXPECT_EQ(result, WALKER_TYPE::SIMD_SIZE::SIMD_SIZE_SIMD32);
    }
};
} // namespace NEO

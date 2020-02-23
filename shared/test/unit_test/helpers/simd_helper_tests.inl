/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/simd_helper.h"
#include "test.h"

namespace NEO {

template <typename WALKER_TYPE>
class GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturned {
  public:
    static void TestBodyImpl() {
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

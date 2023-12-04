/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/simd_helper.h"

namespace NEO {

template <typename DefaultWalkerType>
class GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturned {
  public:
    static void testBodyImpl() {
        uint32_t simd = 32;
        auto result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMD32);

        simd = 16;
        result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMD16);

        simd = 8;
        result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMD8);

        simd = 1;
        result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMD32);
    }
};
} // namespace NEO

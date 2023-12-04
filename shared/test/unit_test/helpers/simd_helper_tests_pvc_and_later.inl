/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/simd_helper.h"

#include "gtest/gtest.h"

namespace NEO {

template <typename DefaultWalkerType>
class GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedPVCAndLater {
  public:
    static void testBodyImpl() {
        uint32_t simd = 32;
        auto result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMT32);

        simd = 16;
        result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMT16);

        simd = 1;
        result = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(result, DefaultWalkerType::SIMD_SIZE::SIMD_SIZE_SIMT32);
    }
};
} // namespace NEO

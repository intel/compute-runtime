/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include <limits>

using namespace NEO;

TEST(CpuVirtualAddressRangeTest, givenPointerWithinUserSpaceRangeWhenValidatingRangeThenReturnTrue) {
    REQUIRE_64BIT_OR_SKIP();

    EXPECT_TRUE(isValidCpuVirtualAddressRange(reinterpret_cast<void *>(0x1000), MemoryConstants::pageSize));
    EXPECT_TRUE(isValidCpuVirtualAddressRange(nullptr, 0u));

    uint32_t stackVariable = 0u;
    EXPECT_TRUE(isValidCpuVirtualAddressRange(&stackVariable, sizeof(stackVariable)));
}

TEST(CpuVirtualAddressRangeTest, givenPointerAboveUserSpaceRangeWhenValidatingRangeThenReturnFalse) {
    REQUIRE_64BIT_OR_SKIP();

    const auto maxCpuVirtualAddress = CpuInfo::getInstance().getMaxCpuVirtualAddress();

    auto lastValidPtr = reinterpret_cast<void *>(maxCpuVirtualAddress);
    EXPECT_TRUE(isValidCpuVirtualAddressRange(lastValidPtr, 1u));
    EXPECT_FALSE(isValidCpuVirtualAddressRange(lastValidPtr, 2u));

    auto gpuOnlyPtr = reinterpret_cast<void *>(maxCpuVirtualAddress + 1);
    EXPECT_FALSE(isValidCpuVirtualAddressRange(gpuOnlyPtr, 1u));
}

TEST(CpuVirtualAddressRangeTest, givenSizeCausingAddressOverflowWhenValidatingRangeThenReturnFalse) {
    REQUIRE_64BIT_OR_SKIP();

    EXPECT_FALSE(isValidCpuVirtualAddressRange(reinterpret_cast<void *>(0x1000), std::numeric_limits<size_t>::max()));
}

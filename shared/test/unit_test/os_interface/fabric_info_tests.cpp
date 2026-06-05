/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/fabric_info.h"

#include "gtest/gtest.h"

#include <chrono>

using namespace NEO;

static constexpr uint64_t nanosecondsPerSecond = std::chrono::nanoseconds::period::den;

TEST(ConvertBandwidthToBytesPerNanosec, GivenBytesPerSecUnitThenDividesByNanosecondsPerSecond) {
    EXPECT_EQ(1u, convertBandwidthToBytesPerNanosec(nanosecondsPerSecond, FabricBandwidthUnit::bytesPerSec));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenBytesPerSecUnitWithLargeBandwidthThenReturnsCorrectValue) {
    uint64_t bandwidth = 100ULL * nanosecondsPerSecond;
    EXPECT_EQ(100u, convertBandwidthToBytesPerNanosec(bandwidth, FabricBandwidthUnit::bytesPerSec));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenBytesPerSecUnitWithBandwidthSmallerThanOneBytePerNanosecThenReturnsZero) {
    EXPECT_EQ(0u, convertBandwidthToBytesPerNanosec(nanosecondsPerSecond - 1, FabricBandwidthUnit::bytesPerSec));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenGbpsUnitThenDividesByBitsPerByte) {
    EXPECT_EQ(1u, convertBandwidthToBytesPerNanosec(bitsPerByte, FabricBandwidthUnit::gbps));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenGbpsUnitWithLargeBandwidthThenReturnsCorrectValue) {
    EXPECT_EQ(100u, convertBandwidthToBytesPerNanosec(100ULL * bitsPerByte, FabricBandwidthUnit::gbps));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenGbpsUnitWithBandwidthLessThanEightThenReturnsZero) {
    EXPECT_EQ(0u, convertBandwidthToBytesPerNanosec(bitsPerByte - 1, FabricBandwidthUnit::gbps));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenUnknownUnitThenReturnsZero) {
    EXPECT_EQ(0u, convertBandwidthToBytesPerNanosec(nanosecondsPerSecond, FabricBandwidthUnit::unknown));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenZeroBandwidthWithBytesPerSecUnitThenReturnsZero) {
    EXPECT_EQ(0u, convertBandwidthToBytesPerNanosec(0u, FabricBandwidthUnit::bytesPerSec));
}

TEST(ConvertBandwidthToBytesPerNanosec, GivenZeroBandwidthWithGbpsUnitThenReturnsZero) {
    EXPECT_EQ(0u, convertBandwidthToBytesPerNanosec(0u, FabricBandwidthUnit::gbps));
}

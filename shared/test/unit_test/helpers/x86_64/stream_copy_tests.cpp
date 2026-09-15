/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cpu_copy_helper.h"
#include "shared/source/helpers/x86_64/stream_copy.h"
#include "shared/source/helpers/x86_64/stream_copy.inl"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/helpers/x86_64/stream_copy_blocks_ult.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> sfenceCounter;
} // namespace CpuIntrinsicsTests

using StreamCopyFn = void (*)(void *, const void *, size_t);

struct AlignmentCase {
    size_t srcOffset;
    size_t dstOffset;
};

struct HwPath {
    StreamCopyFn copyFn;
    size_t blockWidth;
};

namespace {

void resetStreamBlockCounters() {
    NEO::StreamCopyBlocksUlt::reset();
}

constexpr size_t bufferGuardSize = NEO::streamCopyAvx512Width;
constexpr uint8_t bufferGuardValue = 0xDEu;

constexpr size_t tailSizes[] = {
    0u, 1u, 4u, 7u, 8u, 15u, 16u, 31u, 32u, 33u, 49u, 63u, 65u, 129u};

constexpr size_t streamingSizes[] = {
    63u, 127u, 128u, 256u, 1024u};

constexpr size_t dispatchSizes[] = {
    0u, 15u, 16u, 31u, 32u, 127u, 4096u};

constexpr AlignmentCase streamingAlignmentCases[] = {
    {0u, 0u},
    {NEO::streamCopySseWidth, 0u},
    {NEO::streamCopyAvx2Width, 0u},
    {0u, 1u},
    {1u, 0u},
    {3u, 7u},
    {NEO::streamCopySseWidth, 3u},
    {NEO::streamCopyAvx2Width, 7u}};

constexpr uint64_t possibleFeatures[] = {
    NEO::CpuInfo::featureSse41 | NEO::CpuInfo::featureAvX2 | NEO::CpuInfo::featureAvX512,
    NEO::CpuInfo::featureSse41 | NEO::CpuInfo::featureAvX2,
    NEO::CpuInfo::featureSse41,
    NEO::CpuInfo::featureNone};

constexpr HwPath hwPaths[] = {
    {&NEO::streamCopyFromWriteCombinedSse, NEO::streamCopySseWidth},
    {&NEO::streamCopyFromWriteCombinedAvx2, NEO::streamCopyAvx2Width},
    {&NEO::streamCopyFromWriteCombinedAvx512, NEO::streamCopyAvx512Width}};

struct GuardedBuffer {
    decltype(allocateAlignedMemory(0u, 0u)) allocation;
    uint8_t *data;
};

GuardedBuffer allocateGuardedBuffer(size_t dataSize, size_t offset) {
    const size_t totalSize = bufferGuardSize + offset + dataSize + bufferGuardSize;
    auto allocation = allocateAlignedMemory(totalSize, bufferGuardSize);
    std::memset(allocation.get(), bufferGuardValue, totalSize);
    auto *data = static_cast<uint8_t *>(allocation.get()) + bufferGuardSize + offset;
    return {std::move(allocation), data};
}

void runCopyTest(StreamCopyFn copyFn, size_t dataSize, AlignmentCase alignment) {
    auto source = allocateGuardedBuffer(dataSize, alignment.srcOffset);
    auto destination = allocateGuardedBuffer(dataSize, alignment.dstOffset);

    for (size_t i = 0; i < dataSize; ++i) {
        source.data[i] = static_cast<uint8_t>(i);
    }

    copyFn(destination.data, source.data, dataSize);

    EXPECT_EQ(0, std::memcmp(destination.data, source.data, dataSize))
        << "data mismatch, size=" << dataSize
        << ", srcOffset=" << alignment.srcOffset
        << ", dstOffset=" << alignment.dstOffset;

    for (size_t i = 0; i < dataSize; ++i) {
        EXPECT_EQ(static_cast<uint8_t>(i), source.data[i]) << "source modified at byte " << i;
    }

    const size_t srcLeadingSize = bufferGuardSize + alignment.srcOffset;
    const uint8_t *srcLeadingGuard = source.data - srcLeadingSize;
    const uint8_t *srcTrailingGuard = source.data + dataSize;
    for (size_t i = 0; i < srcLeadingSize; ++i) {
        EXPECT_EQ(bufferGuardValue, srcLeadingGuard[i]) << "source modified at byte " << i;
    }
    for (size_t i = 0; i < bufferGuardSize; ++i) {
        EXPECT_EQ(bufferGuardValue, srcTrailingGuard[i]) << "source modified at byte " << i;
    }

    const size_t dstLeadingSize = bufferGuardSize + alignment.dstOffset;
    const uint8_t *dstLeadingGuard = destination.data - dstLeadingSize;
    const uint8_t *dstTrailingGuard = destination.data + dataSize;
    for (size_t i = 0; i < dstLeadingSize; ++i) {
        EXPECT_EQ(bufferGuardValue, dstLeadingGuard[i]) << "destination underflow at byte " << i;
    }
    for (size_t i = 0; i < bufferGuardSize; ++i) {
        EXPECT_EQ(bufferGuardValue, dstTrailingGuard[i]) << "destination overflow at byte " << i;
    }
}

} // namespace

struct StreamCopyDispatchFixture {
    void setUp() {
        auto *mockCpuInfo = getMockCpuInfo(NEO::CpuInfo::getInstance());
        featuresBackup = std::make_unique<VariableBackup<uint64_t>>(&mockCpuInfo->features);
        detectedBackup = std::make_unique<VariableBackup<bool>>(&mockCpuInfo->featuresDetected);
        mockCpuInfo->featuresDetected = true;
    }

    void tearDown() {}

    void forceFeatures(uint64_t features) {
        getMockCpuInfo(NEO::CpuInfo::getInstance())->features = features;
    }

    std::unique_ptr<VariableBackup<uint64_t>> featuresBackup;
    std::unique_ptr<VariableBackup<bool>> detectedBackup;
};

using StreamCopyStreamingPathTest = ::testing::TestWithParam<std::tuple<HwPath, size_t, AlignmentCase>>;

TEST_P(StreamCopyStreamingPathTest, givenWriteCombinedHintThenDataCopiedCorrectly) {
    const auto &[path, size, alignment] = GetParam();
    runCopyTest(path.copyFn, size, alignment);
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyStreamingPathTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths),
                       ::testing::ValuesIn(streamingSizes),
                       ::testing::ValuesIn(streamingAlignmentCases)));

using StreamCopyTailTest = ::testing::TestWithParam<std::tuple<HwPath, size_t, AlignmentCase>>;

TEST_P(StreamCopyTailTest, givenStreamingPathAndSubBlockSizeThenDataCopiedCorrectly) {
    const auto &[path, size, alignment] = GetParam();
    runCopyTest(path.copyFn, size, alignment);
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyTailTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths),
                       ::testing::ValuesIn(tailSizes),
                       ::testing::ValuesIn(streamingAlignmentCases)));

struct StreamCopyDispatchTest : public ::testing::TestWithParam<std::tuple<uint64_t, size_t>>,
                                public StreamCopyDispatchFixture {
    void SetUp() override { StreamCopyDispatchFixture::setUp(); }
    void TearDown() override { StreamCopyDispatchFixture::tearDown(); }
};

TEST_P(StreamCopyDispatchTest, givenDestinationCanBeWriteCombinedThenDataCopiedCorrectly) {
    const auto &[features, size] = GetParam();
    forceFeatures(features);
    runCopyTest([](void *dst, const void *src, size_t bytes) { NEO::streamCopy<true>(dst, src, bytes); }, size, {0u, 3u});
}

TEST_P(StreamCopyDispatchTest, givenDestinationCannotBeWriteCombinedThenDataCopiedCorrectly) {
    const auto &[features, size] = GetParam();
    forceFeatures(features);
    runCopyTest([](void *dst, const void *src, size_t bytes) { NEO::streamCopy<false>(dst, src, bytes); }, size, {1u, 1u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyDispatchTest,
    ::testing::Combine(::testing::ValuesIn(possibleFeatures),
                       ::testing::ValuesIn(dispatchSizes)));

struct StreamCopyCacheSizeFixture {
    void setUp(size_t lastLevelCacheSize) {
        auto *mockCpuInfo = getMockCpuInfo(NEO::CpuInfo::getInstance());
        detectedBackup = std::make_unique<VariableBackup<bool>>(&mockCpuInfo->featuresDetected);
        cacheSizeBackup = std::make_unique<VariableBackup<size_t>>(&mockCpuInfo->lastLevelCacheSize);
        mockCpuInfo->featuresDetected = true;
        mockCpuInfo->lastLevelCacheSize = lastLevelCacheSize;
    }

    std::unique_ptr<VariableBackup<bool>> detectedBackup;
    std::unique_ptr<VariableBackup<size_t>> cacheSizeBackup;
};

using StreamCopyNonTemporalPathTest = ::testing::TestWithParam<std::tuple<HwPath, size_t, AlignmentCase>>;

TEST_P(StreamCopyNonTemporalPathTest, givenSizeAboveCacheBypassLimitThenDataCopiedCorrectlyWithNonTemporalStores) {
    const auto &[path, size, alignment] = GetParam();
    StreamCopyCacheSizeFixture cacheSize;
    cacheSize.setUp(2u * NEO::streamCopyAvx512Width);
    resetStreamBlockCounters();

    runCopyTest(path.copyFn, size, alignment);

    const bool sourceIsBlockAligned = (alignment.srcOffset % path.blockWidth) == 0u;
    const bool destinationIsBlockAligned = (alignment.dstOffset % path.blockWidth) == 0u;

    EXPECT_EQ(0u, NEO::StreamCopyBlocksUlt::misalignedAccessCount);
    EXPECT_LE(size / path.blockWidth,
              NEO::StreamCopyBlocksUlt::streamLoadCount);
    if (sourceIsBlockAligned) {
        const bool expectNonTemporalStores = (size >= NEO::cacheBypassLimit()) && destinationIsBlockAligned;
        EXPECT_EQ(expectNonTemporalStores, NEO::StreamCopyBlocksUlt::streamStoreCount > 0u);
    }
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyNonTemporalPathTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths),
                       ::testing::ValuesIn(streamingSizes),
                       ::testing::ValuesIn(streamingAlignmentCases)));

using StreamCopyUnknownCacheSizeTest = ::testing::TestWithParam<std::tuple<HwPath, size_t>>;

TEST_P(StreamCopyUnknownCacheSizeTest, givenUnknownLastLevelCacheSizeThenDataCopiedCorrectlyWithTemporalStores) {
    const auto &[path, size] = GetParam();
    StreamCopyCacheSizeFixture cacheSize;
    cacheSize.setUp(0u);
    resetStreamBlockCounters();

    runCopyTest(path.copyFn, size, {0u, 0u});

    EXPECT_EQ(0u, NEO::StreamCopyBlocksUlt::streamStoreCount);
    EXPECT_LE(size / path.blockWidth, NEO::StreamCopyBlocksUlt::streamLoadCount);
    EXPECT_EQ(0u, NEO::StreamCopyBlocksUlt::misalignedAccessCount);
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyUnknownCacheSizeTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths),
                       ::testing::ValuesIn(streamingSizes)));

using StreamCopyNonTemporalStoreTest = ::testing::TestWithParam<HwPath>;

TEST_P(StreamCopyNonTemporalStoreTest, givenBlockAlignedBuffersAndSizeAboveCacheBypassLimitThenDataCopiedCorrectlyAndStoreFenceIsEmitted) {
    const auto &path = GetParam();
    StreamCopyCacheSizeFixture cacheSize;
    cacheSize.setUp(2u * NEO::streamCopyAvx512Width);

    constexpr size_t copySize = 4u * MemoryConstants::cacheLineSize;
    ASSERT_LE(NEO::cacheBypassLimit(), copySize);

    CpuIntrinsicsTests::sfenceCounter.store(0u);

    runCopyTest(path.copyFn, copySize, {0u, 0u});

    EXPECT_EQ(1u, CpuIntrinsicsTests::sfenceCounter.load());
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyNonTemporalStoreTest,
    ::testing::ValuesIn(hwPaths));

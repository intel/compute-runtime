/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cpu_copy_helper.h"
#include "shared/source/helpers/x86_64/stream_copy.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

#if defined(_MSC_VER)
#include <immintrin.h>
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

using StreamCopyFn = void (*)(void *, const void *, size_t);

struct AlignmentCase {
    size_t srcOffset;
    size_t dstOffset;
};

struct HwPath {
    StreamCopyFn copyFn;
    bool (*hwSupported)();
};

namespace {

#if !defined(_MSC_VER) && (defined(__GNUC__) || defined(__clang__))
bool osEnabledXsaveMask(unsigned long long mask) {
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
        return false;
    }
    const bool osXsave = (ecx & (1u << 27)) != 0;
    if (!osXsave) {
        return false;
    }
    unsigned int xcr0Lo = 0, xcr0Hi = 0;
    __asm__ __volatile__("xgetbv" : "=a"(xcr0Lo), "=d"(xcr0Hi) : "c"(0));
    const unsigned long long xcr0 = (static_cast<unsigned long long>(xcr0Hi) << 32) | xcr0Lo;
    return (xcr0 & mask) == mask;
}
#endif

bool hwSupportsAvx2() {
#if defined(_MSC_VER)
    int regs1[4] = {};
    __cpuid(regs1, 1);
    const bool osXsave = (regs1[2] & (1 << 27)) != 0;
    if (!osXsave) {
        return false;
    }
    const unsigned long long xcr0 = _xgetbv(0);
    const unsigned long long ymmMask = (1ull << 1) | (1ull << 2);
    if ((xcr0 & ymmMask) != ymmMask) {
        return false;
    }
    int regs7[4] = {};
    __cpuidex(regs7, 7, 0);
    return (regs7[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    const unsigned long long ymmMask = (1ull << 1) | (1ull << 2);
    return (__builtin_cpu_supports("avx2") != 0) && osEnabledXsaveMask(ymmMask);
#else
    return false;
#endif
}

bool hwSupportsAvx512() {
#if defined(_MSC_VER)
    int regs1[4] = {};
    __cpuid(regs1, 1);
    const bool osXsave = (regs1[2] & (1 << 27)) != 0;
    if (!osXsave) {
        return false;
    }
    const unsigned long long xcr0 = _xgetbv(0);
    const unsigned long long avx512Mask = (1ull << 1) | (1ull << 2) | (1ull << 5) | (1ull << 6) | (1ull << 7);
    if ((xcr0 & avx512Mask) != avx512Mask) {
        return false;
    }
    int regs7[4] = {};
    __cpuidex(regs7, 7, 0);
    return (regs7[1] & (1 << 16)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    const unsigned long long avx512Mask = (1ull << 1) | (1ull << 2) | (1ull << 5) | (1ull << 6) | (1ull << 7);
    return (__builtin_cpu_supports("avx512f") != 0) && osEnabledXsaveMask(avx512Mask);
#else
    return false;
#endif
}

bool hwSupportsFeature(uint64_t feature) {
    if (feature == NEO::CpuInfo::featureAvX512) {
        return hwSupportsAvx512();
    }
    if (feature == NEO::CpuInfo::featureAvX2) {
        return hwSupportsAvx2();
    }
    return true;
}

constexpr size_t bufferGuardSize = NEO::streamCopyAvx512Width;
constexpr uint8_t bufferGuardValue = 0xDEu;

constexpr size_t tailSizes[] = {
    0u, 7u, 8u, 15u};

constexpr size_t streamingSizes[] = {
    63u, 127u, 128u, 256u};

constexpr size_t dispatchSizes[] = {
    0u, 127u, 4096u};

constexpr AlignmentCase streamingAlignmentCases[] = {
    {0u, 0u},
    {NEO::streamCopySseWidth, 0u},
    {NEO::streamCopyAvx2Width, 0u},
    {0u, 1u},
    {NEO::streamCopySseWidth, 3u},
    {NEO::streamCopyAvx2Width, 7u}};

constexpr uint64_t possibleFeatures[] = {
    NEO::CpuInfo::featureAvX2,
    NEO::CpuInfo::featureAvX512,
    NEO::CpuInfo::featureNone};

constexpr HwPath hwPaths[] = {
    {&NEO::streamCopyImpl<false>, &hwSupportsAvx2},
    {&NEO::streamCopyImpl<true>, &hwSupportsAvx512}};

constexpr HwPath writeCombinedPaths[] = {
    {&NEO::streamCopyImpl<false, true>, &hwSupportsAvx2},
    {&NEO::streamCopyImpl<false, false>, &hwSupportsAvx2},
    {&NEO::streamCopyImpl<true, true>, &hwSupportsAvx512},
    {&NEO::streamCopyImpl<true, false>, &hwSupportsAvx512}};

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

TEST_P(StreamCopyStreamingPathTest, givenAlignedSourceThenDataCopiedCorrectly) {
    const auto &[path, size, alignment] = GetParam();
    if (!path.hwSupported()) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    runCopyTest(path.copyFn, size, alignment);
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyStreamingPathTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths),
                       ::testing::ValuesIn(streamingSizes),
                       ::testing::ValuesIn(streamingAlignmentCases)));

using StreamCopyTailTest = ::testing::TestWithParam<std::tuple<HwPath, size_t>>;

TEST_P(StreamCopyTailTest, givenStreamingPathAndSubBlockSizeThenDataCopiedCorrectly) {
    const auto &[path, size] = GetParam();
    if (!path.hwSupported()) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    runCopyTest(path.copyFn, size, {0u, 0u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyTailTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths),
                       ::testing::ValuesIn(tailSizes)));

struct StreamCopyDispatchTest : public ::testing::TestWithParam<std::tuple<uint64_t, size_t>>,
                                public StreamCopyDispatchFixture {
    void SetUp() override { StreamCopyDispatchFixture::setUp(); }
    void TearDown() override { StreamCopyDispatchFixture::tearDown(); }
};

TEST_P(StreamCopyDispatchTest, givenAlignedSourceThenDataCopiedCorrectly) {
    const auto &[feature, size] = GetParam();
    if (!hwSupportsFeature(feature)) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    forceFeatures(feature);
    runCopyTest(&NEO::streamCopy, size, {0u, 3u});
}

TEST_P(StreamCopyDispatchTest, givenUnalignedSourceThenDataCopiedViaMemcpy) {
    const auto &[feature, size] = GetParam();
    if (!hwSupportsFeature(feature)) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    forceFeatures(feature);
    runCopyTest(&NEO::streamCopy, size, {1u, 0u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyDispatchTest,
    ::testing::Combine(::testing::ValuesIn(possibleFeatures),
                       ::testing::ValuesIn(dispatchSizes)));

using StreamCopyWriteCombinedTest = ::testing::TestWithParam<std::tuple<HwPath, size_t>>;

TEST_P(StreamCopyWriteCombinedTest, givenDestinationCanBeWriteCombinedFlagThenDataCopiedCorrectly) {
    const auto &[path, size] = GetParam();
    if (!path.hwSupported()) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    runCopyTest(path.copyFn, size, {0u, 0u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyWriteCombinedTest,
    ::testing::Combine(::testing::ValuesIn(writeCombinedPaths),
                       ::testing::ValuesIn(streamingSizes)));

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

struct DispatchPath {
    uint64_t feature;
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

constexpr size_t tailSizes[] = {
    0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
    8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};

constexpr size_t streamingSizes[] = {
    16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u,
    127u, 128u, 129u, 255u, 256u, 512u, 1024u, 4096u};

constexpr size_t allSizes[] = {
    0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
    8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u,
    16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u,
    127u, 128u, 129u, 255u, 256u, 512u, 1024u, 4096u};

constexpr AlignmentCase misalignedCases[] = {
    {1u, 0u}, {0u, 1u}};

constexpr uint64_t forcedFeatures[] = {
    NEO::CpuInfo::featureAvX2,
    NEO::CpuInfo::featureAvX512,
    NEO::CpuInfo::featureNone};

constexpr HwPath hwPaths[] = {
    {&NEO::streamCopyImpl<false>, &hwSupportsAvx2},
    {&NEO::streamCopyImpl<true>, &hwSupportsAvx512}};

constexpr DispatchPath dispatchPaths[] = {
    {NEO::CpuInfo::featureAvX2, &hwSupportsAvx2},
    {NEO::CpuInfo::featureAvX512, &hwSupportsAvx512}};

void runCopyTest(StreamCopyFn copyFn, size_t dataSize, AlignmentCase align) {
    constexpr size_t guardSize = 64u;
    constexpr uint8_t guardValue = 0xDEu;

    auto srcMem = allocateAlignedMemory(dataSize + guardSize, 64u);
    const size_t dstSpan = guardSize + dataSize + guardSize;
    auto dstMem = allocateAlignedMemory(dstSpan + guardSize, 64u);

    auto *src = static_cast<uint8_t *>(srcMem.get()) + align.srcOffset;
    auto *dstStart = static_cast<uint8_t *>(dstMem.get()) + align.dstOffset;
    auto *dst = dstStart + guardSize;

    for (size_t i = 0; i < dataSize; ++i) {
        src[i] = static_cast<uint8_t>(i);
    }
    std::memset(dstStart, guardValue, dstSpan);

    copyFn(dst, src, dataSize);

    EXPECT_EQ(0, std::memcmp(dst, src, dataSize)) << "data mismatch for size " << dataSize;

    for (size_t i = 0; i < guardSize; ++i) {
        EXPECT_EQ(guardValue, dstStart[i]) << "underflow at byte " << i;
        EXPECT_EQ(guardValue, dst[dataSize + i]) << "overflow at byte " << i;
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

struct StreamCopyMisalignedTest : public ::testing::TestWithParam<std::tuple<uint64_t, size_t, AlignmentCase>>,
                                  public StreamCopyDispatchFixture {
    void SetUp() override { StreamCopyDispatchFixture::setUp(); }
    void TearDown() override { StreamCopyDispatchFixture::tearDown(); }
};

TEST_P(StreamCopyMisalignedTest, givenForcedFeatureAndMisalignedBuffersThenDataCopiedCorrectly) {
    const auto &[feature, size, align] = GetParam();
    forceFeatures(feature);
    runCopyTest(&NEO::streamCopy, size, align);
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyMisalignedTest,
    ::testing::Combine(::testing::ValuesIn(forcedFeatures),
                       ::testing::ValuesIn(allSizes),
                       ::testing::ValuesIn(misalignedCases)));

using StreamCopyDispatchSimdTest = ::testing::TestWithParam<std::tuple<DispatchPath, size_t>>;

struct StreamCopyDispatchSimdTestFixture : public StreamCopyDispatchSimdTest,
                                           public StreamCopyDispatchFixture {
    void SetUp() override { StreamCopyDispatchFixture::setUp(); }
    void TearDown() override { StreamCopyDispatchFixture::tearDown(); }
};

TEST_P(StreamCopyDispatchSimdTestFixture, givenForcedFeatureAndAlignedBuffersThenDataCopiedCorrectly) {
    const auto &[path, size] = GetParam();
    if (!path.hwSupported()) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    forceFeatures(path.feature);
    runCopyTest(&NEO::streamCopy, size, {0u, 0u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyDispatchSimdTestFixture,
    ::testing::Combine(::testing::ValuesIn(dispatchPaths), ::testing::ValuesIn(streamingSizes)));

using StreamCopyAlignedStreamingTest = ::testing::TestWithParam<std::tuple<HwPath, size_t>>;

TEST_P(StreamCopyAlignedStreamingTest, givenSimdPathAndStreamingSizeThenDataCopiedCorrectly) {
    const auto &[path, size] = GetParam();
    if (!path.hwSupported()) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    runCopyTest(path.copyFn, size, {0u, 0u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyAlignedStreamingTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths), ::testing::ValuesIn(streamingSizes)));

using StreamCopyAlignedTailTest = ::testing::TestWithParam<std::tuple<HwPath, size_t>>;

TEST_P(StreamCopyAlignedTailTest, givenSimdPathAndTailSizeThenDataCopiedCorrectly) {
    const auto &[path, size] = GetParam();
    if (!path.hwSupported()) {
        GTEST_SKIP() << "AVX feature not supported by this CPU/OS";
    }
    runCopyTest(path.copyFn, size, {0u, 0u});
}

INSTANTIATE_TEST_SUITE_P(
    StreamCopy,
    StreamCopyAlignedTailTest,
    ::testing::Combine(::testing::ValuesIn(hwPaths), ::testing::ValuesIn(tailSizes)));

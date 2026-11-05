/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/memory/memory_aub_fixture.h"

#include "test_mode.h"

#include <tuple>

namespace L0 {
namespace ult {

namespace {
constexpr size_t unifiedMemoryDataSize = 2 * MemoryConstants::pageSize64k;
} // namespace

struct UnifiedMemoryAubTestL0 : public MemoryAubFixture,
                                public ::testing::Test {
    void SetUp() override {
        MemoryAubFixture::setUp();
        this->values = std::vector<char>(unifiedMemoryDataSize, 11);
    }

    void TearDown() override {
        MemoryAubFixture::tearDown();
    }

    std::vector<char> values;
};

HWTEST_F(UnifiedMemoryAubTestL0, givenDeviceMemoryAllocWhenWriteIntoItThenValuesMatch) {
    auto unifiedMemoryPtr = this->allocate(AubMemoryType::deviceUsm, unifiedMemoryDataSize);
    this->initialize(AubMemoryType::deviceUsm, unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);
    this->executeAndSynchronize();

    this->template expectMemory<FamilyType>(unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);
}

HWTEST_F(UnifiedMemoryAubTestL0, givenSharedMemoryAllocWhenWriteIntoCPUPartThenValuesMatchAfterUsingAllocOnGpu) {
    if (!this->isPageFaultManagerAvailable()) {
        GTEST_SKIP();
    }
    auto unifiedMemoryPtr = this->allocate(AubMemoryType::sharedUsm, unifiedMemoryDataSize);
    const uint8_t zeroPattern = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(this->commandList->toHandle(), unifiedMemoryPtr, &zeroPattern, sizeof(zeroPattern), unifiedMemoryDataSize, nullptr, 0, nullptr));
    this->executeAndSynchronize();
    this->commandList->reset();

    this->initialize(AubMemoryType::sharedUsm, unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);

    this->template expectNotEqualMemory<FamilyType>(unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);

    auto hostPtr = this->allocate(AubMemoryType::system, unifiedMemoryDataSize);
    this->appendCopy(hostPtr, unifiedMemoryPtr, unifiedMemoryDataSize);
    this->executeAndSynchronize();

    this->template expectMemory<FamilyType>(unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);
}

HWTEST_F(UnifiedMemoryAubTestL0, givenSharedMemoryAllocWhenWriteIntoGPUPartThenValuesMatchAfterUsingAlloc) {
    if (!this->isPageFaultManagerAvailable()) {
        GTEST_SKIP();
    }
    auto unifiedMemoryPtr = this->allocate(AubMemoryType::sharedUsm, unifiedMemoryDataSize);

    this->appendCopy(unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);
    this->executeAndSynchronize();

    this->template expectMemory<FamilyType>(unifiedMemoryPtr, this->values.data(), unifiedMemoryDataSize);

    if (NEO::testMode == NEO::TestMode::aubTestsWithTbx) {
        this->template expectMemory<FamilyType>(unifiedMemoryPtr, unifiedMemoryPtr, unifiedMemoryDataSize);
    }
}

struct UnifiedMemoryCopyAubTestL0 : public MemoryAubFixture,
                                    public ::testing::TestWithParam<std::tuple<AubMemoryType, AubMemoryType>> {
    void SetUp() override {
        MemoryAubFixture::setUp();
        std::tie(this->srcMemoryType, this->dstMemoryType) = GetParam();
        const bool usesSharedMemory = this->srcMemoryType == AubMemoryType::sharedUsm || this->dstMemoryType == AubMemoryType::sharedUsm;
        if (usesSharedMemory && !this->isPageFaultManagerAvailable()) {
            GTEST_SKIP();
        }

        this->srcPtr = this->allocate(this->srcMemoryType, unifiedMemoryDataSize);
        this->dstPtr = this->allocate(this->dstMemoryType, unifiedMemoryDataSize);

        this->srcValues = std::vector<char>(unifiedMemoryDataSize, 11);
        this->dstValues = std::vector<char>(unifiedMemoryDataSize, 22);

        this->initialize(this->srcMemoryType, this->srcPtr, this->srcValues.data(), unifiedMemoryDataSize);
        this->initialize(this->dstMemoryType, this->dstPtr, this->dstValues.data(), unifiedMemoryDataSize);
    }

    void TearDown() override {
        MemoryAubFixture::tearDown();
    }

    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    AubMemoryType srcMemoryType = AubMemoryType::hostUsm;
    AubMemoryType dstMemoryType = AubMemoryType::hostUsm;
    std::vector<char> srcValues;
    std::vector<char> dstValues;
};

HWTEST_P(UnifiedMemoryCopyAubTestL0, givenTwoUnifiedMemoryAllocsWhenCopyingOneToAnotherThenValuesMatch) {
    this->appendCopy(this->dstPtr, this->srcPtr, unifiedMemoryDataSize);
    this->executeAndSynchronize();

    this->template expectMemory<FamilyType>(this->getGpuPointer(this->dstPtr), this->srcValues.data(), unifiedMemoryDataSize);
}

INSTANTIATE_TEST_SUITE_P(UnifiedMemoryCopyAubTestL0MemoryTypes,
                         UnifiedMemoryCopyAubTestL0,
                         ::testing::Combine(
                             ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::deviceUsm, AubMemoryType::sharedUsm, AubMemoryType::system),
                             ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::deviceUsm, AubMemoryType::sharedUsm, AubMemoryType::system)));

} // namespace ult
} // namespace L0

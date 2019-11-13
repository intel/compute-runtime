/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/aub_tests/fixtures/unified_memory_fixture.h"

namespace NEO {
class UnifiedMemoryCopyAubTest : public UnifiedMemoryAubFixture,
                                 public ::testing::TestWithParam<std::tuple<InternalMemoryType, InternalMemoryType>> {
  public:
    void *srcPtr, *dstPtr;
    InternalMemoryType srcMemoryType, dstMemoryType;
    std::vector<char> srcValues, dstValues;

    void SetUp() override {
        UnifiedMemoryAubFixture::SetUp();

        srcMemoryType = std::get<0>(GetParam());
        dstMemoryType = std::get<1>(GetParam());

        srcPtr = this->allocateUSM(srcMemoryType);
        dstPtr = this->allocateUSM(dstMemoryType);

        srcValues = std::vector<char>(dataSize, 11);
        dstValues = std::vector<char>(dataSize, 22);

        this->writeToUsmMemory(srcValues, srcPtr, srcMemoryType);
        this->writeToUsmMemory(dstValues, dstPtr, dstMemoryType);
    }

    void TearDown() override {
        this->freeUSM(srcPtr, srcMemoryType);
        this->freeUSM(dstPtr, dstMemoryType);
        UnifiedMemoryAubFixture::TearDown();
    }
};

HWTEST_P(UnifiedMemoryCopyAubTest, givenTwoUnifiedMemoryAllocsWhenCopyingOneToAnotherThenValuesMatch) {
    clEnqueueMemcpyINTEL(this->pCmdQ, true, dstPtr, srcPtr, dataSize, 0, nullptr, nullptr);
    expectMemory<FamilyType>(dstPtr, srcValues.data(), dataSize);
}

InternalMemoryType memoryTypes[] = {InternalMemoryType::HOST_UNIFIED_MEMORY,
                                    InternalMemoryType::DEVICE_UNIFIED_MEMORY,
                                    InternalMemoryType::SHARED_UNIFIED_MEMORY,
                                    InternalMemoryType::NOT_SPECIFIED};

INSTANTIATE_TEST_CASE_P(UnifiedMemoryCopyAubTest,
                        UnifiedMemoryCopyAubTest,
                        ::testing::Combine(::testing::ValuesIn(memoryTypes),
                                           ::testing::ValuesIn(memoryTypes)));
} // namespace NEO

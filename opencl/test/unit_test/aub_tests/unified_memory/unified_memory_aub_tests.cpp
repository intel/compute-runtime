/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/unified_memory_fixture.h"
#include "test.h"

namespace NEO {

class UnifiedMemoryAubTest : public UnifiedMemoryAubFixture,
                             public ::testing::Test {
  public:
    using UnifiedMemoryAubFixture::TearDown;

    std::vector<char> values;

    void SetUp() override {
        UnifiedMemoryAubFixture::SetUp();
        values = std::vector<char>(dataSize, 11);
    };
};

HWTEST_F(UnifiedMemoryAubTest, givenDeviceMemoryAllocWhenWriteIntoItThenValuesMatch) {
    auto unifiedMemoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    auto unifiedMemoryPtr = allocateUSM(unifiedMemoryType);
    writeToUsmMemory(values, unifiedMemoryPtr, unifiedMemoryType);

    expectMemory<FamilyType>(unifiedMemoryPtr, values.data(), dataSize);

    freeUSM(unifiedMemoryPtr, unifiedMemoryType);
}

HWTEST_F(UnifiedMemoryAubTest, givenSharedMemoryAllocWhenWriteIntoCPUPartThenValuesMatchAfterUsingAllocAsKernelParam) {
    auto unifiedMemoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;
    auto unifiedMemoryPtr = allocateUSM(unifiedMemoryType);

    writeToUsmMemory(values, unifiedMemoryPtr, unifiedMemoryType);

    expectNotEqualMemory<FamilyType>(unifiedMemoryPtr, values.data(), dataSize);

    auto mockPtr = std::make_unique<char[]>(dataSize);
    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, mockPtr.get(), unifiedMemoryPtr, dataSize, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    expectMemory<FamilyType>(unifiedMemoryPtr, values.data(), dataSize);

    freeUSM(unifiedMemoryPtr, unifiedMemoryType);
}

HWTEST_F(UnifiedMemoryAubTest, givenSharedMemoryAllocWhenWriteIntoGPUPartThenValuesMatchAfterUsingAlloc) {
    auto unifiedMemoryType = InternalMemoryType::SHARED_UNIFIED_MEMORY;

    auto unifiedMemoryPtr = allocateUSM(unifiedMemoryType);
    std::vector<char> input(dataSize, 11);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, unifiedMemoryPtr, input.data(), dataSize, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    expectNotEqualMemory<FamilyType>(unifiedMemoryPtr, unifiedMemoryPtr, dataSize);
    expectMemory<FamilyType>(unifiedMemoryPtr, input.data(), dataSize);

    auto mockRead = reinterpret_cast<char *>(unifiedMemoryPtr)[0];
    mockRead = 0;

    expectMemory<FamilyType>(unifiedMemoryPtr, unifiedMemoryPtr, dataSize);

    freeUSM(unifiedMemoryPtr, unifiedMemoryType);
}
} // namespace NEO

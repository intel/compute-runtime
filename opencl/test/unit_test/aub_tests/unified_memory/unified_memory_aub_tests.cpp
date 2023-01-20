/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/aub_tests/fixtures/unified_memory_fixture.h"

namespace NEO {

class UnifiedMemoryAubTest : public UnifiedMemoryAubFixture,
                             public ::testing::Test {
  public:
    using UnifiedMemoryAubFixture::tearDown;

    std::vector<char> values;

    void SetUp() override {
        UnifiedMemoryAubFixture::setUp();
        values = std::vector<char>(dataSize, 11);
    };

    void TearDown() override {
        UnifiedMemoryAubFixture::tearDown();
    }
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
    retVal = clEnqueueMemsetINTEL(this->pCmdQ, unifiedMemoryPtr, 0, dataSize, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

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

    expectMemory<FamilyType>(unifiedMemoryPtr, unifiedMemoryPtr, dataSize);

    freeUSM(unifiedMemoryPtr, unifiedMemoryType);
}
} // namespace NEO

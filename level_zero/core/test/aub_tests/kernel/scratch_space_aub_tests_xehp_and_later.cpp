/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/aub_tests/kernel/kernel_aub_fixture.h"

#include <vector>

namespace L0 {
namespace ult {

struct AubScratchSpaceForPrivateFixtureL0 : public KernelAubFixtureL0 {
    void setUp() {
        KernelAubFixtureL0::setUp();

        this->kernel = this->createKernel("simple_kernels", "simple_kernel_6", true);
        ASSERT_NE(nullptr, this->kernel);

        this->expectedMemorySize = this->arraySize * this->vectorSize * sizeof(uint32_t);

        this->srcBuffer = static_cast<uint32_t *>(this->allocateHostMemory(this->expectedMemorySize, MemoryConstants::pageSize));
        ASSERT_NE(nullptr, this->srcBuffer);
        const uint32_t valOdd = 0x1;
        const uint32_t valEven = 0x3;
        for (uint32_t i = 0; i < this->arraySize * this->vectorSize; ++i) {
            this->srcBuffer[i] = (i % 2) ? valOdd : valEven;
        }

        uint32_t sumOdd = 0;
        uint32_t sumEven = 0;
        for (uint32_t i = 0; i < this->arraySize; ++i) {
            sumOdd += ((i + this->scalar) + valOdd);
            sumEven += (i + valEven);
        }

        this->dstBuffer = this->allocateHostMemory(this->expectedMemorySize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, this->dstBuffer);
        memset(this->dstBuffer, 0, this->expectedMemorySize);

        this->expectedMemory.resize(this->arraySize * this->vectorSize);
        for (uint32_t i = 0; i < this->arraySize * this->vectorSize; ++i) {
            this->expectedMemory[i] = (i % 2) ? sumOdd : sumEven;
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(void *), &this->dstBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 1, sizeof(void *), &this->srcBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 2, sizeof(uint32_t), &this->scalar));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 3, sizeof(uint32_t), &this->maxIterations1));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 4, sizeof(uint32_t), &this->maxIterations2));
    }

    ze_kernel_handle_t kernel = nullptr;
    const uint32_t arraySize = 32;
    const uint32_t vectorSize = 2;
    const uint32_t lwsSize = 32;
    const uint32_t scalar = 0x4;
    const uint32_t maxIterations1 = 32;
    const uint32_t maxIterations2 = 32;
    size_t expectedMemorySize = 0;
    uint32_t *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    std::vector<uint32_t> expectedMemory;
};

using AubScratchSpaceForPrivateTestL0 = Test<AubScratchSpaceForPrivateFixtureL0>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubScratchSpaceForPrivateTestL0, WhenKernelUsesScratchSpaceForPrivateThenExpectCorrectResults) {
    this->writeHostMemory<FamilyType>(this->srcBuffer);
    this->writeHostMemory<FamilyType>(this->dstBuffer);

    const uint32_t groupSize[3] = {this->lwsSize, 1, 1};
    const uint32_t groupCount[3] = {this->arraySize / this->lwsSize, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    this->expectMemory<FamilyType>(this->getGpuPointer(this->dstBuffer), this->expectedMemory.data(), this->expectedMemorySize);
}

struct AubScratchSpaceForSpillFillFixtureL0 : public KernelAubFixtureL0 {
    void setUp() {
        KernelAubFixtureL0::setUp();

        this->kernel = this->createKernel("simple_spill_fill_kernel", "spill_test", true);
        ASSERT_NE(nullptr, this->kernel);

        this->expectedMemorySize = (this->arraySize * 2 + 1) * sizeof(int32_t) - 4;
        this->offsetMemorySize = 128 * this->arraySize;

        this->srcBuffer = static_cast<int32_t *>(this->allocateHostMemory(this->expectedMemorySize, MemoryConstants::pageSize));
        this->outBuffer = this->allocateHostMemory(this->expectedMemorySize, MemoryConstants::pageSize);
        this->offsetBuffer = this->allocateHostMemory(this->offsetMemorySize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, this->srcBuffer);
        ASSERT_NE(nullptr, this->outBuffer);
        ASSERT_NE(nullptr, this->offsetBuffer);
        memset(this->srcBuffer, 0, this->expectedMemorySize);
        memset(this->outBuffer, 0, this->expectedMemorySize);
        memset(this->offsetBuffer, 0, this->offsetMemorySize);

        this->expectedMemory.assign(this->expectedMemorySize / sizeof(int32_t), 0);
        const int32_t expectedVal1 = 16256;
        const int32_t expectedVal2 = 512;
        for (uint32_t i = 0; i < this->arraySize; ++i) {
            this->srcBuffer[i] = 2;
            this->expectedMemory[i * 2] = expectedVal1;
            this->expectedMemory[i * 2 + 1] = expectedVal2;
        }

        const auto &kernelAttributes = toKernel(this->kernel)->getKernelDescriptor().kernelAttributes;
        EXPECT_NE(0u, kernelAttributes.perThreadScratchSize[0]);
        EXPECT_EQ(128u, kernelAttributes.numGrfRequired);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(void *), &this->srcBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 1, sizeof(void *), &this->outBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 2, sizeof(void *), &this->offsetBuffer));
    }

    ze_kernel_handle_t kernel = nullptr;
    const uint32_t arraySize = 32;
    const uint32_t lwsSize = 32;
    size_t expectedMemorySize = 0;
    size_t offsetMemorySize = 0;
    int32_t *srcBuffer = nullptr;
    void *outBuffer = nullptr;
    void *offsetBuffer = nullptr;
    std::vector<int32_t> expectedMemory;
};

using AubScratchSpaceForSpillFillTestL0 = Test<AubScratchSpaceForSpillFillFixtureL0>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubScratchSpaceForSpillFillTestL0, givenSurfaceStateScratchSpaceEnabledWhenKernelUsesScratchForSpillFillThenExpectCorrectResults) {
    this->writeHostMemory<FamilyType>(this->srcBuffer);
    this->writeHostMemory<FamilyType>(this->outBuffer);
    this->writeHostMemory<FamilyType>(this->offsetBuffer);

    const uint32_t groupSize[3] = {this->lwsSize, 1, 1};
    const uint32_t groupCount[3] = {this->arraySize / this->lwsSize, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    this->expectMemory<FamilyType>(this->getGpuPointer(this->outBuffer), this->expectedMemory.data(), this->expectedMemorySize);
}

} // namespace ult
} // namespace L0

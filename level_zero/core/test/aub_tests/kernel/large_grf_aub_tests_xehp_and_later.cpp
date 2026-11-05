/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/test/aub_tests/kernel/kernel_aub_fixture.h"

#include "test_traits_common.h"

#include <utility>
#include <vector>

namespace L0 {
namespace ult {

struct LargeGrfTestL0 : public KernelAubFixtureL0,
                        public ::testing::TestWithParam<bool> {
    void SetUp() override {
        if (GetParam()) {
            NEO::debugManager.flags.ForceRunAloneContext.set(1);
            NEO::debugManager.flags.AubDumpAddMmioRegistersList.set("0xE48C;0x2020202;0xE4F0;0x20002");
        }
        KernelAubFixtureL0::setUp();
    }

    void TearDown() override {
        KernelAubFixtureL0::tearDown();
    }

    void createKernels(bool largeOnly) {
        this->largeGrfKernel = this->createKernel("spill_fill_kernel_large_grf", "spillFillKernelLargeGRF", true);
        ASSERT_NE(nullptr, this->largeGrfKernel);
        EXPECT_EQ(256u, toKernel(this->largeGrfKernel)->getKernelDescriptor().kernelAttributes.numGrfRequired);

        if (!largeOnly) {
            this->smallGrfKernel = this->createKernel("simple_kernels", "simple_kernel_4", true);
            ASSERT_NE(nullptr, this->smallGrfKernel);
            EXPECT_LE(toKernel(this->smallGrfKernel)->getKernelDescriptor().kernelAttributes.numGrfRequired, 128u);
        }
    }

    template <typename FamilyType>
    void createLargeGrfBuffers() {
        this->indexMemory = static_cast<int32_t *>(this->allocateHostMemory(indexSize, MemoryConstants::pageSize));
        this->sourceMemory = static_cast<int64_t *>(this->allocateHostMemory(bufferSize, MemoryConstants::pageSize));
        this->destinationMemory = static_cast<int64_t *>(this->allocateHostMemory(bufferSize, MemoryConstants::pageSize));
        ASSERT_NE(nullptr, this->indexMemory);
        ASSERT_NE(nullptr, this->sourceMemory);
        ASSERT_NE(nullptr, this->destinationMemory);

        this->largeGrfExpectedMemory.resize(numElems * 16);
        for (int64_t i = 0; i < numElems; i++) {
            this->indexMemory[i] = static_cast<int32_t>(i % 16);
            for (int64_t j = 0; j < 16; j++) {
                const int64_t index = 16 * i + j;
                this->sourceMemory[index] = index;
                this->destinationMemory[index] = 0xdeadbeef;
                this->largeGrfExpectedMemory[index] = (index + 1) * (index + 1);
            }
        }
        this->writeHostMemory<FamilyType>(this->indexMemory);
        this->writeHostMemory<FamilyType>(this->sourceMemory);
        this->writeHostMemory<FamilyType>(this->destinationMemory);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->largeGrfKernel, 0, sizeof(void *), &this->indexMemory));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->largeGrfKernel, 1, sizeof(void *), &this->sourceMemory));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->largeGrfKernel, 2, sizeof(void *), &this->destinationMemory));
    }

    void createSmallGrfBuffers(uint32_t addition) {
        std::vector<uint32_t> smallGrfSourceData(numElems);
        const uint32_t smallGrfDestinationPattern = 0xDEADBEAF;
        this->smallGrfExpectedMemory.resize(numElems);
        for (uint32_t i = 0; i < static_cast<uint32_t>(numElems); i++) {
            smallGrfSourceData[i] = i;
            this->smallGrfExpectedMemory[i] = i + addition;
        }

        this->smallGrfSourceBuffer = this->allocateDeviceMemoryWithData(smallGrfSourceData.data(), numElems * sizeof(uint32_t));
        this->smallGrfDestinationBuffer = this->allocateDeviceMemoryWithPattern(&smallGrfDestinationPattern, sizeof(smallGrfDestinationPattern), numElems * sizeof(uint32_t));
        ASSERT_NE(nullptr, this->smallGrfSourceBuffer);
        ASSERT_NE(nullptr, this->smallGrfDestinationBuffer);
        this->appendBarrier();

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->smallGrfKernel, 0, sizeof(void *), &this->smallGrfSourceBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->smallGrfKernel, 1, sizeof(uint32_t), &addition));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->smallGrfKernel, 2, sizeof(void *), &this->smallGrfDestinationBuffer));
    }

    void appendLargeGrfKernel() {
        const uint32_t requiredGroupSize = toKernel(this->largeGrfKernel)->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[0];
        CmdListNonUniformLaunchArgs launchArgs{};
        launchArgs.workDim = 1;
        launchArgs.globalWorkSize[0] = numElems;
        launchArgs.enqueuedGroupSize[0] = requiredGroupSize;
        EXPECT_EQ(ZE_RESULT_SUCCESS, this->commandList->appendLaunchKernelNonUniform(this->largeGrfKernel, launchArgs, nullptr, 0, nullptr, false));
    }

    void appendSmallGrfKernel() {
        uint32_t groupSize[3] = {1, 1, 1};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSuggestGroupSize(this->smallGrfKernel, numElems, 1, 1, &groupSize[0], &groupSize[1], &groupSize[2]));
        const uint32_t groupCount[3] = {numElems / groupSize[0], 1, 1};
        this->appendLaunch(this->smallGrfKernel, groupSize, groupCount);
    }

    template <typename FamilyType>
    std::vector<bool> getProgrammedLargeGrfValues(size_t queueStreamOffset) {
        using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

        std::vector<bool> largeGrfValues;
        const std::pair<NEO::LinearStream *, size_t> streams[] = {{&this->getQueueStream(), queueStreamOffset},
                                                                  {&this->getCommandListStream(), 0u}};
        for (const auto &stream : streams) {
            NEO::HardwareParse hwParser;
            hwParser.parseCommands<FamilyType>(*stream.first, stream.second);
            for (auto &cmd : hwParser.getCommandsList<STATE_COMPUTE_MODE>()) {
                largeGrfValues.push_back(reinterpret_cast<STATE_COMPUTE_MODE *>(cmd)->getLargeGrfMode());
            }
        }
        return largeGrfValues;
    }

    template <typename FamilyType>
    void verifySingleLargeGrfKernelProgramming(size_t queueStreamOffset) {
        auto largeGrfValues = getProgrammedLargeGrfValues<FamilyType>(queueStreamOffset);
        if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
            ASSERT_FALSE(largeGrfValues.empty());
            for (const auto largeGrfValue : largeGrfValues) {
                EXPECT_TRUE(largeGrfValue);
            }
        } else {
            for (const auto largeGrfValue : largeGrfValues) {
                EXPECT_FALSE(largeGrfValue);
            }
            EXPECT_EQ(256u, NEO::UnitTestHelper<FamilyType>::getProgrammedGrfValue(*this->csr, this->getCommandListStream()));
        }
    }

    static constexpr int32_t numElems = 16;
    static constexpr size_t bufferSize = numElems * sizeof(int64_t) * 16;
    static constexpr size_t indexSize = numElems * sizeof(int32_t);

    ze_kernel_handle_t largeGrfKernel = nullptr;
    ze_kernel_handle_t smallGrfKernel = nullptr;
    int32_t *indexMemory = nullptr;
    int64_t *sourceMemory = nullptr;
    int64_t *destinationMemory = nullptr;
    void *smallGrfSourceBuffer = nullptr;
    void *smallGrfDestinationBuffer = nullptr;
    std::vector<int64_t> largeGrfExpectedMemory;
    std::vector<uint32_t> smallGrfExpectedMemory;
};

HWTEST2_P(LargeGrfTestL0, givenLargeGrfKernelWhenExecutedThenResultsAreCorrect, IsAtLeastXeCore) {
    auto kernel = this->createKernel("simple_kernel_large_grf", "SimpleKernelLargeGRF", true);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(256u, toKernel(kernel)->getKernelDescriptor().kernelAttributes.numGrfRequired);

    const size_t size = numElems * sizeof(uint32_t);
    auto sourceMemory = static_cast<uint32_t *>(this->allocateHostMemory(size, MemoryConstants::pageSize));
    auto destinationMemory = static_cast<uint32_t *>(this->allocateHostMemory(size, MemoryConstants::pageSize));
    ASSERT_NE(nullptr, sourceMemory);
    ASSERT_NE(nullptr, destinationMemory);

    std::vector<uint32_t> expectedMemory(numElems);
    for (uint32_t i = 0; i < static_cast<uint32_t>(numElems); i++) {
        sourceMemory[i] = i;
        destinationMemory[i] = 0xDEADBEEF;
        expectedMemory[i] = i + 1;
    }
    this->writeHostMemory<FamilyType>(sourceMemory);
    this->writeHostMemory<FamilyType>(destinationMemory);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &sourceMemory));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &destinationMemory));

    const auto queueStreamOffset = this->getQueueStream().getUsed();
    const uint32_t groupSize[3] = {numElems, 1, 1};
    const uint32_t groupCount[3] = {1, 1, 1};
    this->appendLaunch(kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    this->verifySingleLargeGrfKernelProgramming<FamilyType>(queueStreamOffset);

    this->expectMemory<FamilyType>(this->getGpuPointer(destinationMemory), expectedMemory.data(), size);
}

HWTEST2_P(LargeGrfTestL0, givenKernelWithSpillWhenExecutedInLargeGrfThenDontSpillAndResultsAreCorrect, IsAtLeastXeCore) {
    this->createKernels(true);
    this->createLargeGrfBuffers<FamilyType>();

    const auto queueStreamOffset = this->getQueueStream().getUsed();
    this->appendLargeGrfKernel();
    this->executeAndSynchronize();

    this->verifySingleLargeGrfKernelProgramming<FamilyType>(queueStreamOffset);

    this->expectMemory<FamilyType>(this->getGpuPointer(this->destinationMemory), this->largeGrfExpectedMemory.data(), bufferSize);
}

HWTEST2_P(LargeGrfTestL0, givenMixedLargeGrfAndSmallGrfKernelsWhenExecutedThenResultsAreCorrect, IsAtLeastXeCore) {
    this->createKernels(false);
    this->createLargeGrfBuffers<FamilyType>();
    const uint32_t addition = 11;
    this->createSmallGrfBuffers(addition);

    const auto queueStreamOffset = this->getQueueStream().getUsed();
    this->appendLargeGrfKernel();
    this->appendSmallGrfKernel();
    this->appendLargeGrfKernel();
    this->appendSmallGrfKernel();
    this->executeAndSynchronize();

    auto largeGrfValues = this->getProgrammedLargeGrfValues<FamilyType>(queueStreamOffset);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        ASSERT_LE(4u, largeGrfValues.size());
        const auto firstChecked = largeGrfValues.size() - 4;
        EXPECT_TRUE(largeGrfValues[firstChecked]);
        EXPECT_FALSE(largeGrfValues[firstChecked + 1]);
        EXPECT_TRUE(largeGrfValues[firstChecked + 2]);
        EXPECT_FALSE(largeGrfValues[firstChecked + 3]);
    } else {
        for (const auto largeGrfValue : largeGrfValues) {
            EXPECT_FALSE(largeGrfValue);
        }
    }

    this->expectMemory<FamilyType>(this->getGpuPointer(this->destinationMemory), this->largeGrfExpectedMemory.data(), bufferSize);
    this->expectMemory<FamilyType>(this->smallGrfDestinationBuffer, this->smallGrfExpectedMemory.data(), numElems * sizeof(uint32_t));
}

INSTANTIATE_TEST_SUITE_P(LargeGrfTestsL0, LargeGrfTestL0, ::testing::Values(true, false));

} // namespace ult
} // namespace L0

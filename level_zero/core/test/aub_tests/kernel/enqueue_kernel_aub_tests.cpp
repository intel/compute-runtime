/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/test/aub_tests/kernel/kernel_aub_fixture.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

namespace L0 {
namespace ult {

struct KernelAubWorkSizeParam {
    uint32_t groupSize[3];
    uint32_t groupCount[3];

    size_t getWorkItemCount() const {
        return static_cast<size_t>(this->groupSize[0]) * this->groupCount[0] * this->groupSize[1] * this->groupCount[1] * this->groupSize[2] * this->groupCount[2];
    }
};

static KernelAubWorkSizeParam kernelAubWorkSizeParamTable[] = {
    {{1, 1, 1}, {1, 1, 1}},
    {{16, 1, 1}, {1, 1, 1}},
    {{16, 1, 1}, {2, 1, 1}},
    {{1, 1, 1}, {64, 1, 1}},
    {{16, 1, 1}, {4, 1, 1}},
    {{64, 1, 1}, {1, 1, 1}}};

static uint32_t kernelAubSimdTable[] = {8, 16, 32};

struct AUBHelloWorldIntegrateTestL0 : public KernelAubFixtureL0,
                                      public ::testing::TestWithParam<std::tuple<uint32_t, KernelAubWorkSizeParam>> {
    void SetUp() override {
        std::tie(this->simd, this->param) = GetParam();
        NEO::MockExecutionEnvironment mockExecutionEnvironment{};
        const auto &hwInfo = *mockExecutionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        if (this->simd < hwInfo.caps.minimalSimdSize) {
            GTEST_SKIP();
        }
        KernelAubFixtureL0::setUp();
    }

    void TearDown() override {
        KernelAubFixtureL0::tearDown();
    }

    KernelAubWorkSizeParam param = {};
    uint32_t simd = 0;
    const size_t sizeUserMemory = 128 * sizeof(float);
    const uint8_t srcPattern = 85;
    const uint8_t destPattern = 170;
};

HWTEST_P(AUBHelloWorldIntegrateTestL0, givenCopyBufferKernelWhenLaunchedThenDestinationContainsSourceAndRemainderIsNotOverwritten) {
    if (this->simd < NEO::UnitTestHelper<FamilyType>::smallestTestableSimdSize) {
        GTEST_SKIP();
    }

    auto srcBuffer = this->allocateDeviceMemoryWithPattern(&this->srcPattern, sizeof(this->srcPattern), this->sizeUserMemory);
    auto destBuffer = this->allocateDeviceMemoryWithPattern(&this->destPattern, sizeof(this->destPattern), this->sizeUserMemory);
    ASSERT_NE(nullptr, srcBuffer);
    ASSERT_NE(nullptr, destBuffer);
    this->appendBarrier();

    auto kernel = this->createKernel("CopyBuffer_simd" + std::to_string(this->simd), "CopyBuffer", true);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &srcBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &destBuffer));

    this->appendLaunch(kernel, this->param.groupSize, this->param.groupCount);
    this->executeAndSynchronize();

    std::vector<uint8_t> expectedMemory(this->sizeUserMemory, this->destPattern);
    std::fill_n(expectedMemory.begin(), this->param.getWorkItemCount() * sizeof(float), this->srcPattern);
    this->expectMemory<FamilyType>(destBuffer, expectedMemory.data(), this->sizeUserMemory);
}

INSTANTIATE_TEST_SUITE_P(
    AUB,
    AUBHelloWorldIntegrateTestL0,
    ::testing::Combine(
        ::testing::ValuesIn(kernelAubSimdTable),
        ::testing::ValuesIn(kernelAubWorkSizeParamTable)));

struct AUBSimpleArgFixtureL0 : public KernelAubFixtureL0 {
    void setUp() {
        KernelAubFixtureL0::setUp();

        this->kernel = this->createKernel("simple_kernels", "simple_arg_int", true);
        ASSERT_NE(nullptr, this->kernel);

        this->destMemory = this->allocateHostMemory(this->sizeUserMemory, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, this->destMemory);
        memset(this->destMemory, this->initialPattern, this->sizeUserMemory);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(this->argVal), &this->argVal));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 1, sizeof(void *), &this->destMemory));
    }

    template <typename FamilyType>
    void launchAndExpect(const KernelAubWorkSizeParam &workSize) {
        this->writeHostMemory<FamilyType>(this->destMemory);

        this->appendLaunch(this->kernel, workSize.groupSize, workSize.groupCount);
        this->executeAndSynchronize();

        std::vector<uint8_t> expectedMemory(this->sizeUserMemory, this->initialPattern);
        std::fill_n(expectedMemory.begin(), workSize.getWorkItemCount() * sizeof(int), this->writtenPattern);
        this->expectMemory<FamilyType>(this->getGpuPointer(this->destMemory), expectedMemory.data(), this->sizeUserMemory);
    }

    ze_kernel_handle_t kernel = nullptr;
    void *destMemory = nullptr;
    const size_t sizeUserMemory = 128 * sizeof(float);
    const uint8_t initialPattern = 0x11;
    const uint8_t writtenPattern = 0x22;
    const int argVal = static_cast<int>(0x22222222);
};

struct AUBSimpleArgIntegrateTestL0 : public AUBSimpleArgFixtureL0,
                                     public ::testing::TestWithParam<KernelAubWorkSizeParam> {
    void SetUp() override {
        AUBSimpleArgFixtureL0::setUp();
    }

    void TearDown() override {
        AUBSimpleArgFixtureL0::tearDown();
    }
};

HWTEST_P(AUBSimpleArgIntegrateTestL0, givenSimpleArgKernelWhenLaunchedThenWrittenMemoryIsCorrectAndRemainderIsNotOverwritten) {
    this->launchAndExpect<FamilyType>(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    AUB,
    AUBSimpleArgIntegrateTestL0,
    ::testing::ValuesIn(kernelAubWorkSizeParamTable));

struct AUBSimpleKernelStatelessFixtureL0 : public KernelAubFixtureL0 {
    void setUp() {
        KernelAubFixtureL0::setUp();
        NEO::debugManager.flags.DisableStatelessToStatefulOptimization.set(true);

        this->kernel = this->createKernel("stateless_kernel", "statelessKernel", true);
        ASSERT_NE(nullptr, this->kernel);
    }

    template <typename FamilyType>
    void launchAndExpect() {
        constexpr size_t bufferSize = MemoryConstants::pageSize;
        std::vector<uint8_t> bufferExpected(bufferSize, 0xCD);

        auto buffer = this->allocateHostMemory(bufferSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, buffer);
        memset(buffer, 0, bufferSize);
        this->writeHostMemory<FamilyType>(buffer);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(void *), &buffer));

        const uint32_t groupSize[3] = {1, 1, 1};
        const uint32_t groupCount[3] = {static_cast<uint32_t>(bufferSize), 1, 1};
        this->appendLaunch(this->kernel, groupSize, groupCount);
        this->executeAndSynchronize();

        this->expectMemory<FamilyType>(this->getGpuPointer(buffer), bufferExpected.data(), bufferSize);
    }

    ze_kernel_handle_t kernel = nullptr;
};

using AUBSimpleKernelStatelessTestL0 = Test<AUBSimpleKernelStatelessFixtureL0>;

HWTEST_F(AUBSimpleKernelStatelessTestL0, givenSimpleKernelWhenStatelessPathIsUsedThenExpectCorrectBuffer) {
    const auto &kernelDescriptor = toKernel(this->kernel)->getKernelDescriptor();
    EXPECT_FALSE(kernelDescriptor.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().isPureStateful());
    EXPECT_EQ(NEO::KernelDescriptor::Stateless, kernelDescriptor.kernelAttributes.bufferAddressingMode);

    this->launchAndExpect<FamilyType>();
}

HWTEST_F(AUBSimpleKernelStatelessTestL0, givenPrefetchEnabledWhenKernelIsLaunchedThenDataIsCorrect) {
    NEO::debugManager.flags.EnableMemoryPrefetch.set(1);

    this->launchAndExpect<FamilyType>();
}

struct AUBSimpleArgNonUniformFixtureL0 : public KernelAubFixtureL0 {
    void setUp() {
        KernelAubFixtureL0::setUp();

        this->kernel = this->createKernel("simple_nonuniform", "simpleNonUniform", true);
        ASSERT_NE(nullptr, this->kernel);
    }

    void prepareOutputBuffer(size_t workItemCount) {
        ASSERT_GT(bufferItems, workItemCount);
        this->maxId = static_cast<int>(workItemCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(this->maxId), &this->maxId));

        this->expectedMemory.assign(bufferItems, this->initialValue);
        std::iota(this->expectedMemory.begin(), this->expectedMemory.begin() + this->maxId + 1, 0);

        this->outBuffer = this->allocateDeviceMemoryWithPattern(&this->initialValue, sizeof(this->initialValue), bufferSize);
        ASSERT_NE(nullptr, this->outBuffer);
        this->appendBarrier();
        const int counterInitialValue = 0;
        this->appendFill(ptrOffset(this->outBuffer, this->maxId * sizeof(int)), &counterInitialValue, sizeof(counterInitialValue), sizeof(counterInitialValue));
        this->appendBarrier();
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 1, sizeof(void *), &this->outBuffer));
    }

    template <typename FamilyType>
    void launchNonUniformAndExpect(uint32_t workDim, const uint32_t globalWorkSize[3], const uint32_t localWorkSize[3], uint32_t expectedWalkerCount) {
        this->prepareOutputBuffer(static_cast<size_t>(globalWorkSize[0]) * globalWorkSize[1] * globalWorkSize[2]);

        CmdListNonUniformLaunchArgs launchArgs{};
        launchArgs.workDim = workDim;
        for (uint32_t dim = 0; dim < 3; dim++) {
            launchArgs.globalWorkSize[dim] = globalWorkSize[dim];
            launchArgs.enqueuedGroupSize[dim] = localWorkSize[dim];
        }
        const auto launchOffset = this->getCommandListStream().getUsed();
        ASSERT_EQ(ZE_RESULT_SUCCESS, this->commandList->appendLaunchKernelNonUniform(this->kernel, launchArgs, nullptr, 0, nullptr, false));

        NEO::HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(this->getCommandListStream(), launchOffset);
        EXPECT_EQ(expectedWalkerCount, hwParser.getCommandWalkerCount<FamilyType>());

        this->executeAndSynchronize();

        this->expectMemory<FamilyType>(this->outBuffer, this->expectedMemory.data(), bufferSize);
    }

    static constexpr size_t bufferItems = 40 * 40 * 40;
    static constexpr size_t bufferSize = bufferItems * sizeof(int);
    ze_kernel_handle_t kernel = nullptr;
    const int initialValue = static_cast<int>(0xdeadbeef);
    int maxId = 0;
    std::vector<int> expectedMemory;
    void *outBuffer = nullptr;
};

using AUBSimpleAtomicTestL0 = Test<AUBSimpleArgNonUniformFixtureL0>;

HWTEST_F(AUBSimpleAtomicTestL0, givenKernelWithAtomicWhenExecutedThenExpectAtomicValueIsCorrect) {
    const uint32_t groupSize[3] = {32, 1, 1};
    const uint32_t groupCount[3] = {2, 1, 1};

    this->prepareOutputBuffer(groupSize[0] * groupCount[0]);

    this->appendLaunch(this->kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    this->expectMemory<FamilyType>(this->outBuffer, this->expectedMemory.data(), bufferSize);
}

using AUBSimpleArgNonUniformTestL0 = Test<AUBSimpleArgNonUniformFixtureL0>;

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork1DimNonUniformGroupThenExpectTwoWalkers) {
    const uint32_t globalWorkSize[3] = {39, 1, 1};
    const uint32_t localWorkSize[3] = {32, 1, 1};
    this->launchNonUniformAndExpect<FamilyType>(1, globalWorkSize, localWorkSize, 2u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork2DimNonUniformGroupInXDimensionThenExpectTwoWalkers) {
    const uint32_t globalWorkSize[3] = {39, 32, 1};
    const uint32_t localWorkSize[3] = {16, 16, 1};
    this->launchNonUniformAndExpect<FamilyType>(2, globalWorkSize, localWorkSize, 2u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork2DimNonUniformGroupInYDimensionThenExpectTwoWalkers) {
    const uint32_t globalWorkSize[3] = {32, 39, 1};
    const uint32_t localWorkSize[3] = {16, 16, 1};
    this->launchNonUniformAndExpect<FamilyType>(2, globalWorkSize, localWorkSize, 2u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork2DimNonUniformGroupInXandYDimensionThenExpectFourWalkers) {
    const uint32_t globalWorkSize[3] = {39, 39, 1};
    const uint32_t localWorkSize[3] = {16, 16, 1};
    this->launchNonUniformAndExpect<FamilyType>(2, globalWorkSize, localWorkSize, 4u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInXDimensionThenExpectTwoWalkers) {
    const uint32_t globalWorkSize[3] = {39, 32, 32};
    const uint32_t localWorkSize[3] = {8, 8, 2};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 2u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInYDimensionThenExpectTwoWalkers) {
    const uint32_t globalWorkSize[3] = {32, 39, 32};
    const uint32_t localWorkSize[3] = {8, 8, 2};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 2u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInZDimensionThenExpectTwoWalkers) {
    const uint32_t globalWorkSize[3] = {32, 32, 39};
    const uint32_t localWorkSize[3] = {8, 2, 8};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 2u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInXandYDimensionThenExpectFourWalkers) {
    const uint32_t globalWorkSize[3] = {39, 39, 32};
    const uint32_t localWorkSize[3] = {8, 8, 2};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 4u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInXandZDimensionThenExpectFourWalkers) {
    const uint32_t globalWorkSize[3] = {39, 32, 39};
    const uint32_t localWorkSize[3] = {8, 2, 8};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 4u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInYandZDimensionThenExpectFourWalkers) {
    const uint32_t globalWorkSize[3] = {32, 39, 39};
    const uint32_t localWorkSize[3] = {2, 8, 8};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 4u);
}

HWTEST_F(AUBSimpleArgNonUniformTestL0, givenNonUniformLaunchWhenProvidingWork3DimNonUniformGroupInXandYandZDimensionThenExpectEightWalkers) {
    const uint32_t globalWorkSize[3] = {39, 39, 39};
    const uint32_t localWorkSize[3] = {8, 8, 2};
    this->launchNonUniformAndExpect<FamilyType>(3, globalWorkSize, localWorkSize, 8u);
}

} // namespace ult
} // namespace L0

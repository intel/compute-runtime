/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using KernelImp = Test<DeviceFixture>;

TEST_F(KernelImp, GivenCrossThreadDataThenIsCorrectlyPatchedWithGlobalWorkSizeAndGroupCount) {
    uint32_t *crossThreadData =
        reinterpret_cast<uint32_t *>(alignedMalloc(sizeof(uint32_t[6]), 32));

    WhiteBox<::L0::KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;
    funcInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[0] = 0 * sizeof(uint32_t);
    funcInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[1] = 1 * sizeof(uint32_t);
    funcInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[2] = 2 * sizeof(uint32_t);
    funcInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[0] = 3 * sizeof(uint32_t);
    funcInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[1] = 4 * sizeof(uint32_t);
    funcInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[2] = 5 * sizeof(uint32_t);

    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.crossThreadData.reset(reinterpret_cast<uint8_t *>(crossThreadData));
    function.crossThreadDataSize = sizeof(uint32_t[6]);
    function.groupSize[0] = 2;
    function.groupSize[1] = 3;
    function.groupSize[2] = 5;

    function.KernelImp::setGroupCount(7, 11, 13);
    auto crossThread = function.KernelImp::getCrossThreadData();
    ASSERT_NE(nullptr, crossThread);
    const uint32_t *globalWorkSizes = reinterpret_cast<const uint32_t *>(crossThread);
    EXPECT_EQ(2U * 7U, globalWorkSizes[0]);
    EXPECT_EQ(3U * 11U, globalWorkSizes[1]);
    EXPECT_EQ(5U * 13U, globalWorkSizes[2]);

    const uint32_t *numGroups = globalWorkSizes + 3;
    EXPECT_EQ(7U, numGroups[0]);
    EXPECT_EQ(11U, numGroups[1]);
    EXPECT_EQ(13U, numGroups[2]);

    function.crossThreadData.release();
    alignedFree(crossThreadData);
}

TEST_F(KernelImp, givenExecutionMaskWithoutReminderWhenProgrammingItsValueThenSetValidNumberOfBits) {
    NEO::KernelDescriptor descriptor = {};
    WhiteBox<KernelImmutableData> funcInfo = {};
    funcInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.module = &module;

    const std::array<uint32_t, 4> testedSimd = {{1, 8, 16, 32}};

    for (auto simd : testedSimd) {
        descriptor.kernelAttributes.simdSize = simd;
        function.KernelImp::setGroupSize(simd, 1, 1);

        if (simd == 1) {
            EXPECT_EQ(maxNBitValue(32), function.KernelImp::getThreadExecutionMask());
        } else {
            EXPECT_EQ(maxNBitValue(simd), function.KernelImp::getThreadExecutionMask());
        }
    }
}

TEST_F(KernelImp, WhenSuggestingGroupSizeThenClampToMaxGroupSize) {
    DebugManagerStateRestore restorer;

    WhiteBox<KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;

    NEO::DebugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);
    module.getMaxGroupSizeResult = 8;

    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.module = &module;
    uint32_t groupSize[3];
    function.KernelImp::suggestGroupSize(256, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(8U, groupSize[0]);
    EXPECT_EQ(1U, groupSize[1]);
    EXPECT_EQ(1U, groupSize[2]);
}

class KernelImpSuggestGroupSize : public DeviceFixture, public ::testing::TestWithParam<uint32_t> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

INSTANTIATE_TEST_CASE_P(, KernelImpSuggestGroupSize,
                        ::testing::Values(4, 7, 8, 16, 32, 192, 1024, 4097, 16000));

TEST_P(KernelImpSuggestGroupSize, WhenSuggestingGroupThenProperGroupSizeChosen) {
    DebugManagerStateRestore restorer;

    WhiteBox<KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;

    NEO::DebugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);

    uint32_t size = GetParam();

    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.module = &module;
    uint32_t groupSize[3];
    function.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    function.KernelImp::suggestGroupSize(size, size, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    function.KernelImp::suggestGroupSize(size, size, size, groupSize, groupSize + 1,
                                         groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    function.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    function.KernelImp::suggestGroupSize(1, size, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    function.KernelImp::suggestGroupSize(1, 1, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    function.KernelImp::suggestGroupSize(1, size, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    function.KernelImp::suggestGroupSize(size, 1, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);
}

TEST_F(KernelImp, GivenInvalidValuesWhenSettingGroupSizeThenInvalidArgumentErrorIsReturned) {
    Mock<Kernel> function;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, function.KernelImp::setGroupSize(0U, 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, function.KernelImp::setGroupSize(1U, 0U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, function.KernelImp::setGroupSize(1U, 1U, 0U));
}

TEST_F(KernelImp, givenSetGroupSizeWithGreaterGroupSizeThanAllowedThenCorrectErrorCodeIsReturned) {
    WhiteBox<KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.module = &module;

    uint32_t maxGroupSizeX = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[0]);
    uint32_t maxGroupSizeY = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[1]);
    uint32_t maxGroupSizeZ = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[2]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, function.KernelImp::setGroupSize(maxGroupSizeX, maxGroupSizeY, maxGroupSizeZ));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, function.KernelImp::setGroupSize(maxGroupSizeX + 1U, 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, function.KernelImp::setGroupSize(1U, maxGroupSizeY + 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, function.KernelImp::setGroupSize(1U, 1U, maxGroupSizeZ + 1U));
}

TEST_F(KernelImp, GivenNumChannelsZeroWhenSettingGroupSizeThenLocalIdsNotGenerated) {
    WhiteBox<KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.module = &module;

    function.KernelImp::setGroupSize(16U, 16U, 1U);
    std::vector<char> memBefore;
    {
        auto perThreadData =
            reinterpret_cast<const char *>(function.KernelImp::getPerThreadData());
        memBefore.assign(perThreadData,
                         perThreadData + function.KernelImp::getPerThreadDataSize());
    }

    function.KernelImp::setGroupSize(8U, 32U, 1U);
    std::vector<char> memAfter;
    {
        auto perThreadData =
            reinterpret_cast<const char *>(function.KernelImp::getPerThreadData());
        memAfter.assign(perThreadData,
                        perThreadData + function.KernelImp::getPerThreadDataSize());
    }

    EXPECT_EQ(memAfter, memBefore);
}

TEST(zeKernelGetProperties, WhenGettingKernelPropertiesThenSuccessIsReturned) {
    Mock<Kernel> kernel;

    ze_kernel_properties_t properties = {};

    auto result = zeKernelGetProperties(kernel.toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

class KernelImpSuggestMaxCooperativeGroupCountTests : public KernelImp {
  public:
    const uint32_t numGrf = 128;
    const uint32_t simd = 8;
    const uint32_t lws[3] = {1, 1, 1};
    uint32_t usedSlm = 0;
    uint32_t usesBarriers = 0;

    uint32_t availableThreadCount;
    uint32_t dssCount;
    uint32_t availableSlm;
    uint32_t maxBarrierCount;
    WhiteBox<::L0::KernelImmutableData> funcInfo;
    NEO::KernelDescriptor kernelDescriptor;

    void SetUp() override {
        KernelImp::SetUp();
        funcInfo.kernelDescriptor = &kernelDescriptor;
        auto &hardwareInfo = device->getHwInfo();
        auto &hwHelper = device->getHwHelper();
        availableThreadCount = hwHelper.calculateAvailableThreadCount(hardwareInfo, numGrf);

        dssCount = hardwareInfo.gtSystemInfo.DualSubSliceCount;
        if (dssCount == 0) {
            dssCount = hardwareInfo.gtSystemInfo.SubSliceCount;
        }
        availableSlm = dssCount * KB * hardwareInfo.capabilityTable.slmSize;
        maxBarrierCount = static_cast<uint32_t>(hwHelper.getMaxBarrierRegisterPerSlice());

        funcInfo.kernelDescriptor->kernelAttributes.simdSize = simd;
        funcInfo.kernelDescriptor->kernelAttributes.numGrfRequired = numGrf;
    }

    uint32_t getMaxWorkGroupCount() {
        funcInfo.kernelDescriptor->kernelAttributes.slmInlineSize = usedSlm;
        funcInfo.kernelDescriptor->kernelAttributes.barrierCount = usesBarriers;

        Mock<Kernel> kernel;
        kernel.kernelImmData = &funcInfo;
        auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
        kernel.module = module.get();

        kernel.groupSize[0] = lws[0];
        kernel.groupSize[1] = lws[1];
        kernel.groupSize[2] = lws[2];
        uint32_t totalGroupCount = 0;
        kernel.KernelImp::suggestMaxCooperativeGroupCount(&totalGroupCount, NEO::EngineGroupType::CooperativeCompute, true);
        return totalGroupCount;
    }
};

TEST_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenNoBarriersOrSlmUsedWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithSimd) {
    auto workGroupSize = lws[0] * lws[1] * lws[2];
    auto expected = availableThreadCount / Math::divideAndRoundUp(workGroupSize, simd);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenBarriersWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithRegardToBarriersCount) {
    usesBarriers = 1;
    auto expected = dssCount * (maxBarrierCount / usesBarriers);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenUsedSlmSizeWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithRegardToUsedSlmSize) {
    usedSlm = 64 * KB;
    auto expected = availableSlm / usedSlm;
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

} // namespace ult
} // namespace L0

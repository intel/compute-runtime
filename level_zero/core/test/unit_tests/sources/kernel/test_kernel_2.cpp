/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
namespace NEO {
extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];
}
namespace L0 {
#include "level_zero/core/source/kernel/patch_with_implicit_surface.inl"
namespace ult {

using KernelImp = Test<DeviceFixture>;

TEST_F(KernelImp, GivenCrossThreadDataThenIsCorrectlyPatchedWithGlobalWorkSizeAndGroupCount) {
    uint32_t *crossThreadData =
        reinterpret_cast<uint32_t *>(alignedMalloc(sizeof(uint32_t[6]), 32));

    WhiteBox<::L0::KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[0] = 0 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[1] = 1 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[2] = 2 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[0] = 3 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[1] = 4 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[2] = 5 * sizeof(uint32_t);

    Mock<Kernel> kernel;
    kernel.kernelImmData = &kernelInfo;
    kernel.crossThreadData.reset(reinterpret_cast<uint8_t *>(crossThreadData));
    kernel.crossThreadDataSize = sizeof(uint32_t[6]);
    kernel.groupSize[0] = 2;
    kernel.groupSize[1] = 3;
    kernel.groupSize[2] = 5;

    kernel.KernelImp::setGroupCount(7, 11, 13);
    auto crossThread = kernel.KernelImp::getCrossThreadData();
    ASSERT_NE(nullptr, crossThread);
    const uint32_t *globalWorkSizes = reinterpret_cast<const uint32_t *>(crossThread);
    EXPECT_EQ(2U * 7U, globalWorkSizes[0]);
    EXPECT_EQ(3U * 11U, globalWorkSizes[1]);
    EXPECT_EQ(5U * 13U, globalWorkSizes[2]);

    const uint32_t *numGroups = globalWorkSizes + 3;
    EXPECT_EQ(7U, numGroups[0]);
    EXPECT_EQ(11U, numGroups[1]);
    EXPECT_EQ(13U, numGroups[2]);

    kernel.crossThreadData.release();
    alignedFree(crossThreadData);
}

TEST_F(KernelImp, givenExecutionMaskWithoutReminderWhenProgrammingItsValueThenSetValidNumberOfBits) {
    NEO::KernelDescriptor descriptor = {};
    WhiteBox<KernelImmutableData> kernelInfo = {};
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> kernel;
    kernel.kernelImmData = &kernelInfo;
    kernel.module = &module;

    const std::array<uint32_t, 4> testedSimd = {{1, 8, 16, 32}};

    for (auto simd : testedSimd) {
        descriptor.kernelAttributes.simdSize = simd;
        kernel.KernelImp::setGroupSize(simd, 1, 1);

        if (simd == 1) {
            EXPECT_EQ(maxNBitValue(32), kernel.KernelImp::getThreadExecutionMask());
        } else {
            EXPECT_EQ(maxNBitValue(simd), kernel.KernelImp::getThreadExecutionMask());
        }
    }
}

TEST_F(KernelImp, WhenSuggestingGroupSizeThenClampToMaxGroupSize) {
    DebugManagerStateRestore restorer;

    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    NEO::DebugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);
    module.getMaxGroupSizeResult = 8;

    Mock<Kernel> kernel;
    kernel.kernelImmData = &kernelInfo;
    kernel.module = &module;
    uint32_t groupSize[3];
    kernel.KernelImp::suggestGroupSize(256, 1, 1, groupSize, groupSize + 1, groupSize + 2);
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

    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    NEO::DebugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);

    uint32_t size = GetParam();

    Mock<Kernel> kernel;
    kernel.kernelImmData = &kernelInfo;
    kernel.module = &module;
    uint32_t groupSize[3];
    kernel.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, size, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, size, size, groupSize, groupSize + 1,
                                       groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(1, size, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(1, 1, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(1, size, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, 1, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);
}

TEST_P(KernelImpSuggestGroupSize, WhenSlmSizeExceedsLocalMemorySizeAndSuggestingGroupSizeThenDebugMsgErrIsPrintedAndOutOfDeviceMemoryIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    WhiteBox<KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);

    uint32_t size = GetParam();

    Mock<Kernel> function;
    function.kernelImmData = &funcInfo;
    function.module = &module;
    uint32_t groupSize[3];
    EXPECT_EQ(ZE_RESULT_SUCCESS, function.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2));

    auto localMemSize = static_cast<uint32_t>(device->getNEODevice()->getDeviceInfo().localMemSize);

    ::testing::internal::CaptureStderr();

    funcInfo.kernelDescriptor->kernelAttributes.slmInlineSize = localMemSize - 10u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, function.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2));

    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);

    ::testing::internal::CaptureStderr();

    funcInfo.kernelDescriptor->kernelAttributes.slmInlineSize = localMemSize + 10u;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, function.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2));

    output = testing::internal::GetCapturedStderr();
    const auto &slmInlineSize = funcInfo.kernelDescriptor->kernelAttributes.slmInlineSize;
    std::string expectedOutput = "Size of SLM (" + std::to_string(slmInlineSize) + ") larger than available (" + std::to_string(localMemSize) + ")\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(KernelImp, GivenInvalidValuesWhenSettingGroupSizeThenInvalidArgumentErrorIsReturned) {
    Mock<Kernel> kernel;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, kernel.KernelImp::setGroupSize(0U, 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, kernel.KernelImp::setGroupSize(1U, 0U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, kernel.KernelImp::setGroupSize(1U, 1U, 0U));
}

TEST_F(KernelImp, givenSetGroupSizeWithGreaterGroupSizeThanAllowedThenCorrectErrorCodeIsReturned) {
    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> kernel;
    kernel.kernelImmData = &kernelInfo;
    kernel.module = &module;

    uint32_t maxGroupSizeX = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[0]);
    uint32_t maxGroupSizeY = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[1]);
    uint32_t maxGroupSizeZ = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[2]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(maxGroupSizeX, maxGroupSizeY, maxGroupSizeZ));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(maxGroupSizeX + 1U, 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(1U, maxGroupSizeY + 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(1U, 1U, maxGroupSizeZ + 1U));
}

TEST_F(KernelImp, GivenNumChannelsZeroWhenSettingGroupSizeThenLocalIdsNotGenerated) {
    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> kernel;
    kernel.kernelImmData = &kernelInfo;
    kernel.module = &module;

    kernel.KernelImp::setGroupSize(16U, 16U, 1U);
    std::vector<char> memBefore;
    {
        auto perThreadData =
            reinterpret_cast<const char *>(kernel.KernelImp::getPerThreadData());
        memBefore.assign(perThreadData,
                         perThreadData + kernel.KernelImp::getPerThreadDataSize());
    }

    kernel.KernelImp::setGroupSize(8U, 32U, 1U);
    std::vector<char> memAfter;
    {
        auto perThreadData =
            reinterpret_cast<const char *>(kernel.KernelImp::getPerThreadData());
        memAfter.assign(perThreadData,
                        perThreadData + kernel.KernelImp::getPerThreadDataSize());
    }

    EXPECT_EQ(memAfter, memBefore);
}

HWTEST_F(KernelImp, givenSurfaceStateHeapWhenPatchWithImplicitSurfaceCalledThenIsDebuggerActiveIsSetCorrectly) {
    struct MockHwHelper : NEO::HwHelperHw<FamilyType> {
        void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) override {
            savedSurfaceStateArgs = args;
            ++encodeBufferSurfaceStateCalled;
        }
        EncodeSurfaceStateArgs savedSurfaceStateArgs;
        size_t encodeBufferSurfaceStateCalled{0};
    };
    MockHwHelper hwHelper{};
    auto hwInfo = *defaultHwInfo.get();
    VariableBackup<HwHelper *> hwHelperFactoryBackup{&NEO::hwHelperFactory[static_cast<size_t>(hwInfo.platform.eRenderCoreFamily)]};
    hwHelperFactoryBackup = &hwHelper;

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto surfaceStateHeap = std::make_unique<uint8_t[]>(2 * sizeof(RENDER_SURFACE_STATE));
    auto crossThreadDataArrayRef = ArrayRef<uint8_t>();
    auto surfaceStateHeapArrayRef = ArrayRef<uint8_t>(surfaceStateHeap.get(), 2 * sizeof(RENDER_SURFACE_STATE));
    uintptr_t ptrToPatchInCrossThreadData = 0;
    NEO::MockGraphicsAllocation globalBuffer;
    ArgDescPointer ptr;
    ASSERT_EQ(hwHelper.encodeBufferSurfaceStateCalled, 0u);
    {
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false, false);
        EXPECT_EQ(hwHelper.encodeBufferSurfaceStateCalled, 0u);
    }
    {
        ptr.bindful = 1;
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false, false);
        ASSERT_EQ(hwHelper.encodeBufferSurfaceStateCalled, 1u);
        EXPECT_FALSE(hwHelper.savedSurfaceStateArgs.isDebuggerActive);
    }
    {
        neoDevice->setDebuggerActive(true);
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false, false);
        ASSERT_EQ(hwHelper.encodeBufferSurfaceStateCalled, 2u);
        EXPECT_TRUE(hwHelper.savedSurfaceStateArgs.isDebuggerActive);
    }
    {
        neoDevice->setDebuggerActive(false);
        auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
        neoDevice->getRootDeviceEnvironmentRef().debugger.reset(debugger);
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false, false);
        ASSERT_EQ(hwHelper.encodeBufferSurfaceStateCalled, 3u);
        EXPECT_TRUE(hwHelper.savedSurfaceStateArgs.isDebuggerActive);
    }
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
    WhiteBox<::L0::KernelImmutableData> kernelInfo;
    NEO::KernelDescriptor kernelDescriptor;

    void SetUp() override {
        KernelImp::SetUp();
        kernelInfo.kernelDescriptor = &kernelDescriptor;
        auto &hardwareInfo = device->getHwInfo();
        auto &hwHelper = device->getHwHelper();
        availableThreadCount = hwHelper.calculateAvailableThreadCount(hardwareInfo, numGrf);

        dssCount = hardwareInfo.gtSystemInfo.DualSubSliceCount;
        if (dssCount == 0) {
            dssCount = hardwareInfo.gtSystemInfo.SubSliceCount;
        }
        availableSlm = dssCount * KB * hardwareInfo.capabilityTable.slmSize;
        maxBarrierCount = static_cast<uint32_t>(hwHelper.getMaxBarrierRegisterPerSlice());

        kernelInfo.kernelDescriptor->kernelAttributes.simdSize = simd;
        kernelInfo.kernelDescriptor->kernelAttributes.numGrfRequired = numGrf;
    }

    uint32_t getMaxWorkGroupCount() {
        kernelInfo.kernelDescriptor->kernelAttributes.slmInlineSize = usedSlm;
        kernelInfo.kernelDescriptor->kernelAttributes.barrierCount = usesBarriers;

        Mock<Kernel> kernel;
        kernel.kernelImmData = &kernelInfo;
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

using KernelTest = Test<DeviceFixture>;
HWTEST2_F(KernelTest, GivenInlineSamplersWhenSettingInlineSamplerThenDshIsPatched, SupportsSampler) {
    WhiteBox<::L0::KernelImmutableData> kernelImmData = {};
    NEO::KernelDescriptor descriptor;
    kernelImmData.kernelDescriptor = &descriptor;

    auto &inlineSampler = descriptor.inlineSamplers.emplace_back();
    inlineSampler.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::Repeat;
    inlineSampler.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::Nearest;
    inlineSampler.isNormalized = false;

    Mock<Module> module(device, nullptr);
    Mock<Kernel> kernel;
    kernel.module = &module;
    kernel.kernelImmData = &kernelImmData;
    kernel.dynamicStateHeapData.reset(new uint8_t[64 + 16]);
    kernel.dynamicStateHeapDataSize = 64 + 16;

    kernel.setInlineSamplers();

    using SamplerState = typename FamilyType::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<const SamplerState *>(kernel.dynamicStateHeapData.get() + 64U);
    EXPECT_TRUE(samplerState->getNonNormalizedCoordinateEnable());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTcxAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTcyAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTczAddressControlMode());
    EXPECT_EQ(SamplerState::MIN_MODE_FILTER_NEAREST, samplerState->getMinModeFilter());
    EXPECT_EQ(SamplerState::MAG_MODE_FILTER_NEAREST, samplerState->getMagModeFilter());
}

} // namespace ult
} // namespace L0

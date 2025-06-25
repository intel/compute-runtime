/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/kernel_info_from_patchtokens.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/kernel/kernel_hw.h"
#include "level_zero/core/source/kernel/sampler_patch_values.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/source/sampler/sampler_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace NEO {
void populatePointerKernelArg(KernelDescriptor &desc, ArgDescPointer &dst,
                              CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful, CrossThreadDataOffset bindless,
                              KernelDescriptor::AddressingMode addressingMode);
}

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBoxKernelHw : public KernelHw<gfxCoreFamily> {
    using BaseClass = KernelHw<gfxCoreFamily>;
    using BaseClass::BaseClass;
    using ::L0::KernelImp::argumentsResidencyContainer;
    using ::L0::KernelImp::createPrintfBuffer;
    using ::L0::KernelImp::crossThreadData;
    using ::L0::KernelImp::crossThreadDataSize;
    using ::L0::KernelImp::dynamicStateHeapData;
    using ::L0::KernelImp::dynamicStateHeapDataSize;
    using ::L0::KernelImp::groupSize;
    using ::L0::KernelImp::internalResidencyContainer;
    using ::L0::KernelImp::isBindlessOffsetSet;
    using ::L0::KernelImp::kernelImmData;
    using ::L0::KernelImp::kernelRequiresGenerationOfLocalIdsByRuntime;
    using ::L0::KernelImp::module;
    using ::L0::KernelImp::numThreadsPerThreadGroup;
    using ::L0::KernelImp::patchBindlessSurfaceState;
    using ::L0::KernelImp::perThreadDataForWholeThreadGroup;
    using ::L0::KernelImp::perThreadDataSize;
    using ::L0::KernelImp::perThreadDataSizeForWholeThreadGroup;
    using ::L0::KernelImp::printfBuffer;
    using ::L0::KernelImp::requiredWorkgroupOrder;
    using ::L0::KernelImp::surfaceStateHeapData;
    using ::L0::KernelImp::surfaceStateHeapDataSize;
    using ::L0::KernelImp::unifiedMemoryControls;
    using ::L0::KernelImp::usingSurfaceStateHeap;

    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}

    WhiteBoxKernelHw() : ::L0::KernelHw<gfxCoreFamily>(nullptr) {}
};

using KernelInitTest = Test<ModuleImmutableDataFixture>;

TEST_F(KernelInitTest, givenKernelToInitWhenItHasUnknownArgThenUnknowKernelArgHandlerAssigned) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    kernel->initialize(&desc);
    EXPECT_EQ(kernel->kernelArgHandlers[0], &KernelImp::setArgUnknown);
    EXPECT_EQ(mockKernelImmData->getDescriptor().payloadMappings.explicitArgs[0].type, NEO::ArgDescriptor::argTUnknown);
}

TEST_F(KernelInitTest, givenKernelToInitAndPreemptionEnabledWhenItHasUnknownArgThenUnknowKernelArgHandlerAssigned) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    module.get()->getDevice()->getNEODevice()->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper);

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);

    kernel->initialize(&desc);
    EXPECT_EQ(kernel->kernelArgHandlers[0], &KernelImp::setArgUnknown);
    EXPECT_EQ(mockKernelImmData->getDescriptor().payloadMappings.explicitArgs[0].type, NEO::ArgDescriptor::argTUnknown);
}

TEST_F(KernelInitTest, givenKernelToInitAndPreemptionDisabledWhenItHasUnknownArgThenUnknowKernelArgHandlerAssigned) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    module.get()->getDevice()->getNEODevice()->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper);

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);

    kernel->initialize(&desc);
    EXPECT_EQ(kernel->kernelArgHandlers[0], &KernelImp::setArgUnknown);
    EXPECT_EQ(mockKernelImmData->getDescriptor().payloadMappings.explicitArgs[0].type, NEO::ArgDescriptor::argTUnknown);
}

TEST_F(KernelInitTest, givenKernelToInitWhenItHasTooBigPrivateSizeThenOutOfMemoryIsRetutned) {
    auto globalSize = device->getNEODevice()->getRootDevice()->getGlobalMemorySize(static_cast<uint32_t>(device->getNEODevice()->getDeviceBitfield().to_ulong()));
    uint32_t perHwThreadPrivateMemorySizeRequested = (static_cast<uint32_t>((globalSize + device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch) / device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch)) + 100;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    EXPECT_EQ(kernel->initialize(&desc), ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

TEST_F(KernelInitTest, givenKernelToInitWhenItHasTooBigScratchSizeThenInvalidBinaryIsRetutned) {
    auto globalSize = device->getNEODevice()->getRootDevice()->getGlobalMemorySize(static_cast<uint32_t>(device->getNEODevice()->getDeviceBitfield().to_ulong()));
    uint32_t perHwThreadPrivateMemorySizeRequested = (static_cast<uint32_t>((globalSize + device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch) / device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch)) / 2;

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &productHelper = device->getProductHelper();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize(productHelper);
    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested, maxScratchSize + 1, 0x100);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    EXPECT_EQ(kernel->initialize(&desc), ZE_RESULT_ERROR_INVALID_NATIVE_BINARY);
}

TEST_F(KernelInitTest, givenKernelToInitWhenPrivateSurfaceAllocationFailsThenOutOfDeviceMemoryIsRetutned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);
    ::testing::internal::CaptureStderr();

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);

    std::unique_ptr<NEO::MemoryManager> otherMemoryManager = std::make_unique<NEO::FailMemoryManager>(0, *device->getNEODevice()->getExecutionEnvironment());
    device->getNEODevice()->getExecutionEnvironment()->memoryManager.swap(otherMemoryManager);

    EXPECT_EQ(kernel->initialize(&desc), ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);

    device->getNEODevice()->getExecutionEnvironment()->memoryManager.swap(otherMemoryManager);

    auto output = ::testing::internal::GetCapturedStderr();
    std::string errorMsg = "Failed to allocate private surface";
    EXPECT_NE(std::string::npos, output.find(errorMsg));
}

using KernelBaseAddressTests = Test<ModuleImmutableDataFixture>;
TEST_F(KernelBaseAddressTests, whenQueryingKernelBaseAddressThenCorrectAddressIsReturned) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    kernel->initialize(&desc);

    uint64_t baseAddress = 0;
    ze_result_t res = kernel->getBaseAddress(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = kernel->getBaseAddress(&baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(baseAddress, 0u);
    EXPECT_EQ(baseAddress, kernel->getImmutableData()->getIsaGraphicsAllocation()->getGpuAddress());
}

TEST(KernelArgTest, givenKernelWhenSetArgUnknownCalledThenSuccessRteurned) {
    Mock<KernelImp> mockKernel;
    EXPECT_EQ(mockKernel.setArgUnknown(0, 0, nullptr), ZE_RESULT_SUCCESS);
}

struct MockKernelWithCallTracking : Mock<::L0::KernelImp> {
    using ::L0::KernelImp::kernelArgInfos;

    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *peerAllocData) override {
        ++setArgBufferWithAllocCalled;
        return KernelImp::setArgBufferWithAlloc(argIndex, argVal, allocation, peerAllocData);
    }
    size_t setArgBufferWithAllocCalled = 0u;

    ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) override {
        if (this->groupSize[0] == groupSizeX &&
            this->groupSize[1] == groupSizeY &&
            this->groupSize[2] == groupSizeZ) {
            setGroupSizeSkipCount++;
        } else {
            setGroupSizeSkipCount = 0u;
        }
        return KernelImp::setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    }
    size_t setGroupSizeSkipCount = 0u;
};

using SetKernelArgCacheTest = Test<ModuleFixture>;

TEST_F(SetKernelArgCacheTest, givenValidBufferArgumentWhenSetMultipleTimesThenSetArgBufferWithAllocOnlyCalledIfNeeded) {
    MockKernelWithCallTracking mockKernel;
    mockKernel.module = module.get();
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernel.initialize(&desc);

    const auto &kernelArgInfos = mockKernel.getKernelArgInfos();

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto allocationProperties = NEO::SVMAllocsManager::SvmAllocationProperties{};
    auto svmAllocation = svmAllocsManager->createSVMAlloc(4096, allocationProperties, context->rootDeviceIndices, context->deviceBitfields);
    auto allocData = svmAllocsManager->getSVMAlloc(svmAllocation);

    size_t callCounter = 0u;
    svmAllocsManager->allocationsCounter = 0u;
    // first setArg - called
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    // same setArg but allocationCounter == 0 - called
    ASSERT_EQ(svmAllocsManager->allocationsCounter, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    // same setArg and allocId matches - not called
    svmAllocsManager->allocationsCounter = 2u;
    ASSERT_EQ(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, 0u);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, kernelArgInfos[0].allocIdMemoryManagerCounter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, 2u);

    allocData->setAllocId(1u);
    // same setArg but allocId is uninitialized - called
    mockKernel.kernelArgInfos[0].allocId = SvmAllocationData::uninitializedAllocId;
    ASSERT_EQ(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, svmAllocsManager->allocationsCounter);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, kernelArgInfos[0].allocIdMemoryManagerCounter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocId, 1u);

    ++svmAllocsManager->allocationsCounter;
    // same setArg - not called and argInfo.allocationCounter is updated
    EXPECT_EQ(2u, mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_EQ(svmAllocsManager->allocationsCounter, mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter);

    // same setArg and allocationCounter - not called
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);

    allocData->setAllocId(2u);
    ++svmAllocsManager->allocationsCounter;
    ASSERT_NE(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, svmAllocsManager->allocationsCounter);
    ASSERT_NE(mockKernel.kernelArgInfos[0].allocId, allocData->getAllocId());
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocId, kernelArgInfos[0].allocId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, svmAllocsManager->allocationsCounter);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].allocId, allocData->getAllocId());

    const auto &argumentsResidencyContainer = mockKernel.getArgumentsResidencyContainer();
    // different value - called
    auto secondSvmAllocation = svmAllocsManager->createSVMAlloc(4096, allocationProperties, context->rootDeviceIndices, context->deviceBitfields);
    svmAllocsManager->getSVMAlloc(secondSvmAllocation)->setAllocId(3u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(secondSvmAllocation), &secondSvmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_NE(nullptr, argumentsResidencyContainer[0]);

    // nullptr - not called, argInfo is updated
    EXPECT_FALSE(mockKernel.kernelArgInfos[0].isSetToNullptr);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].isSetToNullptr, kernelArgInfos[0].isSetToNullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(nullptr), nullptr));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_TRUE(mockKernel.kernelArgInfos[0].isSetToNullptr);
    EXPECT_EQ(nullptr, argumentsResidencyContainer[0]);

    // nullptr again - not called
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(nullptr), nullptr));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_TRUE(mockKernel.kernelArgInfos[0].isSetToNullptr);

    // same value as before nullptr - called, argInfo is updated
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(secondSvmAllocation), &secondSvmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_FALSE(mockKernel.kernelArgInfos[0].isSetToNullptr);
    EXPECT_NE(nullptr, argumentsResidencyContainer[0]);

    // allocations counter == 0 called
    svmAllocsManager->allocationsCounter = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(secondSvmAllocation), &secondSvmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    // same value but no svmData - ZE_RESULT_SUCCESS with allocId as 0
    svmAllocsManager->freeSVMAlloc(secondSvmAllocation);
    ++svmAllocsManager->allocationsCounter;
    ASSERT_GT(mockKernel.kernelArgInfos[0].allocId, 0u);
    ASSERT_LT(mockKernel.kernelArgInfos[0].allocId, SvmAllocationData::uninitializedAllocId);
    ASSERT_EQ(mockKernel.kernelArgInfos[0].value, secondSvmAllocation);
    EXPECT_EQ(mockKernel.kernelArgInfos[0].value, kernelArgInfos[0].value);
    ASSERT_GT(svmAllocsManager->allocationsCounter, 0u);
    ASSERT_NE(mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter, svmAllocsManager->allocationsCounter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(secondSvmAllocation), &secondSvmAllocation));
    ASSERT_EQ(mockKernel.kernelArgInfos[0].value, secondSvmAllocation);
    ASSERT_EQ(mockKernel.kernelArgInfos[0].allocId, 0u);
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_EQ(nullptr, argumentsResidencyContainer[0]);

    svmAllocsManager->freeSVMAlloc(svmAllocation);
}

using KernelImpSetGroupSizeTest = Test<DeviceFixture>;

TEST_F(KernelImpSetGroupSizeTest, givenLocalIdGenerationByRuntimeEnabledWhenSettingGroupSizeThenProperlyGenerateLocalIds) {
    Mock<KernelImp> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    mockKernel.descriptor.kernelAttributes.simdSize = 1;
    mockKernel.kernelRequiresGenerationOfLocalIdsByRuntime = true; // although it is enabled for SIMD 1, make sure it is enforced
    mockKernel.descriptor.kernelAttributes.numLocalIdChannels = 3;
    mockKernel.module = &mockModule;
    const auto &device = mockModule.getDevice();
    auto grfSize = device->getHwInfo().capabilityTable.grfSize;
    auto numGrf = GrfConfig::defaultGrfNumber;
    const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    uint32_t groupSize[3] = {2, 3, 5};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    const auto &gfxHelper = mockModule.getDevice()->getGfxCoreHelper();
    auto numThreadsPerTG = gfxHelper.calculateNumThreadsPerThreadGroup(
        mockKernel.descriptor.kernelAttributes.simdSize,
        groupSize[0] * groupSize[1] * groupSize[2],
        numGrf,
        rootDeviceEnvironment);
    auto perThreadDataSizeForWholeTGNeeded =
        static_cast<uint32_t>(NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(
            mockKernel.descriptor.kernelAttributes.simdSize,
            grfSize,
            numGrf,
            mockKernel.descriptor.kernelAttributes.numLocalIdChannels,
            groupSize[0] * groupSize[1] * groupSize[2],
            rootDeviceEnvironment));

    EXPECT_EQ(numThreadsPerTG, mockKernel.getNumThreadsPerThreadGroup());
    EXPECT_EQ((perThreadDataSizeForWholeTGNeeded / numThreadsPerTG), mockKernel.perThreadDataSize);

    using LocalIdT = unsigned short;
    auto threadOffsetInLocalIds = grfSize / sizeof(LocalIdT);
    auto generatedLocalIds = reinterpret_cast<LocalIdT *>(mockKernel.perThreadDataForWholeThreadGroup);

    uint32_t threadId = 0;
    for (uint32_t z = 0; z < groupSize[2]; ++z) {
        for (uint32_t y = 0; y < groupSize[1]; ++y) {
            for (uint32_t x = 0; x < groupSize[0]; ++x) {
                EXPECT_EQ(x, generatedLocalIds[0 + threadId * threadOffsetInLocalIds]) << " thread : " << threadId;
                EXPECT_EQ(y, generatedLocalIds[1 + threadId * threadOffsetInLocalIds]) << " thread : " << threadId;
                EXPECT_EQ(z, generatedLocalIds[2 + threadId * threadOffsetInLocalIds]) << " thread : " << threadId;
                ++threadId;
            }
        }
    }
}

TEST_F(KernelImpSetGroupSizeTest, givenLocalIdGenerationByRuntimeDisabledWhenSettingGroupSizeThenLocalIdsAreNotGenerated) {
    Mock<KernelImp> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    mockKernel.descriptor.kernelAttributes.simdSize = 1;
    mockKernel.module = &mockModule;
    mockKernel.kernelRequiresGenerationOfLocalIdsByRuntime = false;

    uint32_t groupSize[3] = {2, 3, 5};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(0u, mockKernel.perThreadDataSizeForWholeThreadGroup);
    EXPECT_EQ(0u, mockKernel.perThreadDataSize);
    EXPECT_EQ(nullptr, mockKernel.perThreadDataForWholeThreadGroup);
}

TEST_F(KernelImpSetGroupSizeTest, givenIncorrectGroupSizeDimensionWhenSettingGroupSizeThenInvalidGroupSizeDimensionErrorIsReturned) {
    Mock<KernelImp> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    for (auto i = 0u; i < 3u; i++) {
        mockKernel.descriptor.kernelAttributes.requiredWorkgroupSize[i] = 2;
    }
    mockKernel.module = &mockModule;

    uint32_t groupSize[3] = {1, 1, 1};
    mockKernel.groupSize[0] = 0;
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, ret);
}

TEST_F(KernelImpSetGroupSizeTest, givenZeroGroupSizeWhenSettingGroupSizeThenInvalidArgumentErrorIsReturned) {
    Mock<KernelImp> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    for (auto i = 0u; i < 3u; i++) {
        mockKernel.descriptor.kernelAttributes.requiredWorkgroupSize[i] = 2;
    }
    mockKernel.module = &mockModule;

    uint32_t groupSize[3] = {0, 0, 0};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
}

TEST_F(KernelImpSetGroupSizeTest, givenValidGroupSizeWhenSetMultipleTimesThenSetGroupSizeIsOnlyExecutedIfNeeded) {
    MockKernelWithCallTracking mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    mockKernel.module = &mockModule;

    // First call with {2u, 3u, 5u} group size - don't skip setGroupSize execution
    auto ret = mockKernel.setGroupSize(2u, 3u, 5u);
    EXPECT_EQ(2u, mockKernel.groupSize[0]);
    EXPECT_EQ(3u, mockKernel.groupSize[1]);
    EXPECT_EQ(5u, mockKernel.groupSize[2]);
    EXPECT_EQ(0u, mockKernel.setGroupSizeSkipCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    // Second call with {2u, 3u, 5u} group size - skip setGroupSize execution
    ret = mockKernel.setGroupSize(2u, 3u, 5u);
    EXPECT_EQ(2u, mockKernel.groupSize[0]);
    EXPECT_EQ(3u, mockKernel.groupSize[1]);
    EXPECT_EQ(5u, mockKernel.groupSize[2]);
    EXPECT_EQ(1u, mockKernel.setGroupSizeSkipCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    // First call with {1u, 2u, 3u} group size - don't skip setGroupSize execution
    ret = mockKernel.setGroupSize(1u, 2u, 3u);
    EXPECT_EQ(1u, mockKernel.groupSize[0]);
    EXPECT_EQ(2u, mockKernel.groupSize[1]);
    EXPECT_EQ(3u, mockKernel.groupSize[2]);
    EXPECT_EQ(0u, mockKernel.setGroupSizeSkipCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    // Second call with {1u, 2u, 3u} group size - skip setGroupSize execution
    ret = mockKernel.setGroupSize(1u, 2u, 3u);
    EXPECT_EQ(1u, mockKernel.groupSize[0]);
    EXPECT_EQ(2u, mockKernel.groupSize[1]);
    EXPECT_EQ(3u, mockKernel.groupSize[2]);
    EXPECT_EQ(1u, mockKernel.setGroupSizeSkipCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

using SetKernelArg = Test<ModuleFixture>;

struct ImageSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= IGFX_SKYLAKE && NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};

HWTEST2_F(SetKernelArg, givenImageAndKernelWhenSetArgImageThenCrossThreadDataIsSet, ImageSupport) {
    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].as<NEO::ArgDescImage>());
    imageArg.metadataPayload.imgWidth = 0x1c;
    imageArg.metadataPayload.imgHeight = 0x18;
    imageArg.metadataPayload.imgDepth = 0x14;

    imageArg.metadataPayload.arraySize = 0x10;
    imageArg.metadataPayload.numSamples = 0xc;
    imageArg.metadataPayload.channelDataType = 0x8;
    imageArg.metadataPayload.channelOrder = 0x4;
    imageArg.metadataPayload.numMipLevels = 0x0;

    imageArg.metadataPayload.flatWidth = 0x30;
    imageArg.metadataPayload.flatHeight = 0x2c;
    imageArg.metadataPayload.flatPitch = 0x28;
    imageArg.metadataPayload.flatBaseOffset = 0x20;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto handle = imageHW->toHandle();
    auto imgInfo = imageHW->getImageInfo();
    auto pixelSize = imgInfo.surfaceFormat->imageElementSizeInBytes;

    kernel->setArgImage(3, sizeof(imageHW.get()), &handle);

    auto crossThreadData = kernel->getCrossThreadData();

    auto pImgWidth = ptrOffset(crossThreadData, imageArg.metadataPayload.imgWidth);
    EXPECT_EQ(imgInfo.imgDesc.imageWidth, *pImgWidth);

    auto pImgHeight = ptrOffset(crossThreadData, imageArg.metadataPayload.imgHeight);
    EXPECT_EQ(imgInfo.imgDesc.imageHeight, *pImgHeight);

    auto pImgDepth = ptrOffset(crossThreadData, imageArg.metadataPayload.imgDepth);
    EXPECT_EQ(imgInfo.imgDesc.imageDepth, *pImgDepth);

    auto pArraySize = ptrOffset(crossThreadData, imageArg.metadataPayload.arraySize);
    EXPECT_EQ(imgInfo.imgDesc.imageArraySize, *pArraySize);

    auto pNumSamples = ptrOffset(crossThreadData, imageArg.metadataPayload.numSamples);
    EXPECT_EQ(imgInfo.imgDesc.numSamples, *pNumSamples);

    auto pNumMipLevels = ptrOffset(crossThreadData, imageArg.metadataPayload.numMipLevels);
    EXPECT_EQ(imgInfo.imgDesc.numMipLevels, *pNumMipLevels);

    auto pFlatBaseOffset = ptrOffset(crossThreadData, imageArg.metadataPayload.flatBaseOffset);
    EXPECT_EQ(imageHW->getAllocation()->getGpuAddress(), *reinterpret_cast<const uint64_t *>(pFlatBaseOffset));

    auto pFlatWidth = ptrOffset(crossThreadData, imageArg.metadataPayload.flatWidth);
    EXPECT_EQ((imgInfo.imgDesc.imageWidth * pixelSize) - 1u, *pFlatWidth);

    auto pFlatHeight = ptrOffset(crossThreadData, imageArg.metadataPayload.flatHeight);
    EXPECT_EQ((imgInfo.imgDesc.imageHeight * pixelSize) - 1u, *pFlatHeight);

    auto pFlatPitch = ptrOffset(crossThreadData, imageArg.metadataPayload.flatPitch);
    EXPECT_EQ(imgInfo.imgDesc.imageRowPitch - 1u, *pFlatPitch);

    auto pChannelDataType = ptrOffset(crossThreadData, imageArg.metadataPayload.channelDataType);
    EXPECT_EQ(getClChannelDataType(desc.format), *reinterpret_cast<const cl_channel_type *>(pChannelDataType));

    auto pChannelOrder = ptrOffset(crossThreadData, imageArg.metadataPayload.channelOrder);
    EXPECT_EQ(getClChannelOrder(desc.format), *reinterpret_cast<const cl_channel_order *>(pChannelOrder));
}

HWTEST2_F(SetKernelArg, givenImageAndKernelFromNativeWhenSetArgImageCalledThenSuccessAndInvalidChannelType, ImageSupport) {
    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].as<NEO::ArgDescImage>());
    imageArg.metadataPayload.imgWidth = 0x1c;
    imageArg.metadataPayload.imgHeight = 0x18;
    imageArg.metadataPayload.imgDepth = 0x14;

    imageArg.metadataPayload.arraySize = 0x10;
    imageArg.metadataPayload.numSamples = 0xc;
    imageArg.metadataPayload.channelDataType = 0x8;
    imageArg.metadataPayload.channelOrder = 0x4;
    imageArg.metadataPayload.numMipLevels = 0x0;

    imageArg.metadataPayload.flatWidth = 0x30;
    imageArg.metadataPayload.flatHeight = 0x2c;
    imageArg.metadataPayload.flatPitch = 0x28;
    imageArg.metadataPayload.flatBaseOffset = 0x20;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto handle = imageHW->toHandle();
    L0::ModuleImp *moduleImp = (L0::ModuleImp *)(module.get());
    EXPECT_FALSE(moduleImp->isSPIRv());

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->setArgImage(3, sizeof(imageHW.get()), &handle));

    auto crossThreadData = kernel->getCrossThreadData();

    auto pChannelDataType = ptrOffset(crossThreadData, imageArg.metadataPayload.channelDataType);
    int channelDataType = (int)(*reinterpret_cast<const cl_channel_type *>(pChannelDataType));
    EXPECT_EQ(CL_INVALID_VALUE, channelDataType);
}

HWTEST2_F(SetKernelArg, givenImageAndKernelFromSPIRvWhenSetArgImageCalledThenUnsupportedReturned, ImageSupport) {
    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].as<NEO::ArgDescImage>());
    imageArg.metadataPayload.imgWidth = 0x1c;
    imageArg.metadataPayload.imgHeight = 0x18;
    imageArg.metadataPayload.imgDepth = 0x14;

    imageArg.metadataPayload.arraySize = 0x10;
    imageArg.metadataPayload.numSamples = 0xc;
    imageArg.metadataPayload.channelDataType = 0x8;
    imageArg.metadataPayload.channelOrder = 0x4;
    imageArg.metadataPayload.numMipLevels = 0x0;

    imageArg.metadataPayload.flatWidth = 0x30;
    imageArg.metadataPayload.flatHeight = 0x2c;
    imageArg.metadataPayload.flatPitch = 0x28;
    imageArg.metadataPayload.flatBaseOffset = 0x20;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto handle = imageHW->toHandle();

    WhiteBox<::L0::Module> *moduleImp = whiteboxCast(module.get());
    moduleImp->builtFromSpirv = true;
    EXPECT_TRUE(moduleImp->isSPIRv());
    kernel->module = moduleImp;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT, kernel->setArgImage(3, sizeof(imageHW.get()), &handle));
}

HWTEST2_F(SetKernelArg, givenSamplerAndKernelWhenSetArgSamplerThenCrossThreadDataIsSet, ImageSupport) {
    createKernel();

    auto &samplerArg = const_cast<NEO::ArgDescSampler &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[5].as<NEO::ArgDescSampler>());
    samplerArg.metadataPayload.samplerAddressingMode = 0x0;
    samplerArg.metadataPayload.samplerNormalizedCoords = 0x4;
    samplerArg.metadataPayload.samplerSnapWa = 0x8;

    ze_sampler_desc_t desc = {};

    desc.addressMode = ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    desc.filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST;
    desc.isNormalized = true;

    auto sampler = std::make_unique<WhiteBox<::L0::SamplerCoreFamily<FamilyType::gfxCoreFamily>>>();

    auto ret = sampler->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto handle = sampler->toHandle();

    kernel->setArgSampler(5, sizeof(sampler.get()), &handle);

    auto crossThreadData = kernel->getCrossThreadData();

    auto pSamplerSnapWa = ptrOffset(crossThreadData, samplerArg.metadataPayload.samplerSnapWa);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), *reinterpret_cast<const uint32_t *>(pSamplerSnapWa));

    auto pSamplerAddressingMode = ptrOffset(crossThreadData, samplerArg.metadataPayload.samplerAddressingMode);
    EXPECT_EQ(static_cast<uint32_t>(SamplerPatchValues::addressClampToBorder), *pSamplerAddressingMode);

    auto pSamplerNormalizedCoords = ptrOffset(crossThreadData, samplerArg.metadataPayload.samplerNormalizedCoords);
    EXPECT_EQ(static_cast<uint32_t>(SamplerPatchValues::normalizedCoordsTrue), *pSamplerNormalizedCoords);
}

TEST_F(SetKernelArg, givenBufferArgumentWhichHasNotBeenAllocatedByRuntimeThenSuccessIsReturned) {
    createKernel();

    uint64_t hostAddress = 0x1234;
    ze_result_t res = kernel->setArgBuffer(0, sizeof(hostAddress), &hostAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(SetKernelArg, givenDisableSystemPointerKernelArgumentIsEnabledWhenBufferArgumentisNotAllocatedByRuntimeThenErrorIsReturned) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableSystemPointerKernelArgument.set(1);
    createKernel();

    uint64_t hostAddress = 0x1234;
    ze_result_t res = kernel->setArgBuffer(0, sizeof(hostAddress), &hostAddress);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

HWTEST2_F(SetKernelArg, givenBindlessImageAndKernelFromNativeWhenSetArgImageCalledThenResidencyContainerHasSingleImplicitArgAllocation, ImageSupport) {
    auto neoDevice = device->getNEODevice();
    if (!neoDevice->getRootDeviceEnvironment().getReleaseHelper() ||
        !neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    constexpr uint32_t imageArgIndex = 3;
    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[imageArgIndex].as<NEO::ArgDescImage>());
    imageArg.metadataPayload.imgWidth = 0x1c;
    imageArg.metadataPayload.imgHeight = 0x18;
    imageArg.metadataPayload.imgDepth = 0x14;

    imageArg.metadataPayload.arraySize = 0x10;
    imageArg.metadataPayload.numSamples = 0xc;
    imageArg.metadataPayload.channelDataType = 0x8;
    imageArg.metadataPayload.channelOrder = 0x4;
    imageArg.metadataPayload.numMipLevels = 0x0;

    imageArg.metadataPayload.flatWidth = 0x30;
    imageArg.metadataPayload.flatHeight = 0x2c;
    imageArg.metadataPayload.flatPitch = 0x28;
    imageArg.metadataPayload.flatBaseOffset = 0x20;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageBasic = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto ret = imageBasic->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto imageBasicHandle = imageBasic->toHandle();

    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {};
    bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
    bindlessExtDesc.pNext = nullptr;
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

    desc = {};
    desc.pNext = &bindlessExtDesc;

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageBindless1 = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ret = imageBindless1->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imgImplicitArgsAlloc1 = imageBindless1->getImplicitArgsAllocation();
    auto imageBindlessHandle1 = imageBindless1->toHandle();

    auto imageBindless2 = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ret = imageBindless2->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imgImplicitArgsAlloc2 = imageBindless2->getImplicitArgsAllocation();
    auto imageBindlessHandle2 = imageBindless2->toHandle();

    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel->implicitArgsResidencyContainerIndices[imageArgIndex]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->setArgImage(imageArgIndex, sizeof(imageBindless1.get()), &imageBindlessHandle1));

    auto implicitArgIt = std::find(kernel->argumentsResidencyContainer.begin(), kernel->argumentsResidencyContainer.end(), imgImplicitArgsAlloc1);
    ASSERT_NE(kernel->argumentsResidencyContainer.end(), implicitArgIt);
    auto expectedDistance = static_cast<size_t>(std::distance(kernel->argumentsResidencyContainer.begin(), implicitArgIt));
    EXPECT_EQ(expectedDistance, kernel->implicitArgsResidencyContainerIndices[imageArgIndex]);
    EXPECT_EQ(imgImplicitArgsAlloc1, kernel->argumentsResidencyContainer[kernel->implicitArgsResidencyContainerIndices[imageArgIndex]]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->setArgImage(imageArgIndex, sizeof(imageBindless2.get()), &imageBindlessHandle2));

    implicitArgIt = std::find(kernel->argumentsResidencyContainer.begin(), kernel->argumentsResidencyContainer.end(), imgImplicitArgsAlloc2);
    ASSERT_NE(kernel->argumentsResidencyContainer.end(), implicitArgIt);
    auto expectedDistance2 = static_cast<size_t>(std::distance(kernel->argumentsResidencyContainer.begin(), implicitArgIt));
    EXPECT_EQ(expectedDistance2, kernel->implicitArgsResidencyContainerIndices[imageArgIndex]);
    EXPECT_EQ(expectedDistance, expectedDistance2);
    EXPECT_EQ(imgImplicitArgsAlloc2, kernel->argumentsResidencyContainer[kernel->implicitArgsResidencyContainerIndices[imageArgIndex]]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->setArgImage(imageArgIndex, sizeof(imageBasic.get()), &imageBasicHandle));

    EXPECT_EQ(nullptr, kernel->argumentsResidencyContainer[kernel->implicitArgsResidencyContainerIndices[imageArgIndex]]);
}

using KernelImmutableDataTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelImmutableDataTests, givenKernelInitializedWithNoPrivateMemoryThenPrivateMemoryIsNull) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 0u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());

    EXPECT_EQ(nullptr, kernel->privateMemoryGraphicsAllocation);
}

TEST_F(KernelImmutableDataTests, givenKernelInitializedWithPrivateMemoryThenPrivateMemoryIsCreated) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());

    EXPECT_NE(nullptr, kernel->privateMemoryGraphicsAllocation);

    size_t expectedSize = alignUp(perHwThreadPrivateMemorySizeRequested *
                                      device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  MemoryConstants::pageSize);
    EXPECT_EQ(expectedSize, kernel->privateMemoryGraphicsAllocation->getUnderlyingBufferSize());
}

using KernelImmutableDataIsaCopyTests = KernelImmutableDataTests;

TEST_F(KernelImmutableDataIsaCopyTests, whenUserKernelIsCreatedThenIsaIsCopiedWhenModuleIsCreated) {
    MockImmutableMemoryManager *mockMemoryManager =
        static_cast<MockImmutableMemoryManager *>(device->getNEODevice()->getMemoryManager());

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    size_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    auto additionalSections = {ZebinTestData::AppendElfAdditionalSection::global};
    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get(), additionalSections);

    size_t copyForGlobalSurface = 1u;
    auto copyForIsa = module->getKernelsIsaParentAllocation() ? 1u : static_cast<uint32_t>(module->getKernelImmutableDataVector().size());
    size_t expectedPreviouscopyMemoryToAllocationCalledTimes = previouscopyMemoryToAllocationCalledTimes +
                                                               copyForGlobalSurface + copyForIsa;
    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes,
              mockMemoryManager->copyMemoryToAllocationCalledTimes);

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());

    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes,
              mockMemoryManager->copyMemoryToAllocationCalledTimes);
}

TEST_F(KernelImmutableDataIsaCopyTests, whenImmutableDataIsInitializedForUserKernelThenIsaIsNotCopied) {
    MockImmutableMemoryManager *mockMemoryManager =
        static_cast<MockImmutableMemoryManager *>(device->getNEODevice()->getMemoryManager());

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    uint32_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    mockKernelImmData->initialize(mockKernelImmData->mockKernelInfo, device,
                                  device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  module->translationUnit->globalConstBuffer,
                                  module->translationUnit->globalVarBuffer,
                                  isInternal);

    EXPECT_EQ(previouscopyMemoryToAllocationCalledTimes,
              mockMemoryManager->copyMemoryToAllocationCalledTimes);
}

TEST_F(KernelImmutableDataIsaCopyTests, whenImmutableDataIsInitializedForInternalKernelThenIsaIsNotCopied) {
    MockImmutableMemoryManager *mockMemoryManager =
        static_cast<MockImmutableMemoryManager *>(device->getNEODevice()->getMemoryManager());

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = true;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    uint32_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    mockKernelImmData->initialize(mockKernelImmData->mockKernelInfo, device,
                                  device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  module->translationUnit->globalConstBuffer,
                                  module->translationUnit->globalVarBuffer,
                                  isInternal);

    EXPECT_EQ(previouscopyMemoryToAllocationCalledTimes,
              mockMemoryManager->copyMemoryToAllocationCalledTimes);
}

using KernelImmutableDataWithNullHeapTests = KernelImmutableDataTests;

TEST_F(KernelImmutableDataTests, givenInternalModuleWhenKernelIsCreatedThenIsaIsCopiedOnce) {
    MockImmutableMemoryManager *mockMemoryManager =
        static_cast<MockImmutableMemoryManager *>(device->getNEODevice()->getMemoryManager());

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = true;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    mockKernelImmData->getIsaGraphicsAllocation()->setAllocationType(AllocationType::kernelIsaInternal);

    size_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    auto additionalSections = {ZebinTestData::AppendElfAdditionalSection::global};
    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get(), additionalSections);

    size_t copyForGlobalSurface = 1u;
    size_t copyForPatchingIsa = module->getKernelsIsaParentAllocation() ? 1u : 0u;
    size_t expectedPreviouscopyMemoryToAllocationCalledTimes = previouscopyMemoryToAllocationCalledTimes +
                                                               copyForGlobalSurface + copyForPatchingIsa;
    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes,
              mockMemoryManager->copyMemoryToAllocationCalledTimes);

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    expectedPreviouscopyMemoryToAllocationCalledTimes++;

    createKernel(kernel.get());

    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes,
              mockMemoryManager->copyMemoryToAllocationCalledTimes);
}

struct KernelIsaCopyingMomentTest : public ModuleImmutableDataFixture, public ::testing::TestWithParam<std::pair<uint32_t, size_t>> {
    void SetUp() override {
        ModuleImmutableDataFixture::setUp();
    }

    void TearDown() override {
        ModuleImmutableDataFixture::tearDown();
    }
};
std::pair<uint32_t, size_t> kernelIsaCopyingPairs[] = {
    {1, 1},
    {static_cast<uint32_t>(MemoryConstants::pageSize64k + 1), 0}}; // pageSize64 is a common upper-bound for both system and local memory

INSTANTIATE_TEST_SUITE_P(, KernelIsaCopyingMomentTest, testing::ValuesIn(kernelIsaCopyingPairs));

TEST_P(KernelIsaCopyingMomentTest, givenInternalModuleWhenKernelIsCreatedThenIsaCopiedDuringLinkingOnlyIfCanFitInACommonParentPage) {
    auto [testKernelHeapSize, numberOfCopiesToAllocationAtModuleInitialization] = GetParam();

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    MockImmutableMemoryManager *mockMemoryManager = static_cast<MockImmutableMemoryManager *>(device->getNEODevice()->getMemoryManager());

    uint8_t binary[16];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfGlobalVariablesBuffer = true;

    std::unique_ptr<L0::ult::MockModule> moduleMock = std::make_unique<L0::ult::MockModule>(device, moduleBuildLog, ModuleType::builtin);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->programInfo.linkerInput = std::move(linkerInput);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = testKernelHeapSize;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelMock.immutableData.surfaceStateHeapSize = 64;
    kernelMock.immutableData.surfaceStateHeapTemplate.reset(new uint8_t[64]);
    kernelMock.immutableData.getIsaGraphicsAllocation()->setAllocationType(AllocationType::kernelIsaInternal);
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    moduleMock->kernelImmData = &kernelMock.immutableData;

    size_t previouscopyMemoryToAllocationCalledTimes = mockMemoryManager->copyMemoryToAllocationCalledTimes;
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleMock->initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    size_t expectedPreviouscopyMemoryToAllocationCalledTimes = previouscopyMemoryToAllocationCalledTimes +
                                                               numberOfCopiesToAllocationAtModuleInitialization;

    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes, mockMemoryManager->copyMemoryToAllocationCalledTimes);

    for (auto &ki : moduleMock->kernelImmDatas) {
        bool isaExpectedToBeCopied = (numberOfCopiesToAllocationAtModuleInitialization != 0u);
        EXPECT_EQ(isaExpectedToBeCopied, ki->isIsaCopiedToAllocation());
    }

    if (numberOfCopiesToAllocationAtModuleInitialization == 0) {
        // For large builtin kernels copying is not optimized and done at kernel initailization
        expectedPreviouscopyMemoryToAllocationCalledTimes++;
    }

    ze_kernel_desc_t desc = {};
    desc.pKernelName = "";

    moduleMock->kernelImmData = moduleMock->kernelImmDatas[0].get();

    kernelMock.initialize(&desc);

    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes, mockMemoryManager->copyMemoryToAllocationCalledTimes);
}

TEST_F(KernelImmutableDataTests, givenKernelInitializedWithPrivateMemoryThenContainerHasOneExtraSpaceForAllocation) {
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();
    ModuleBuildLog *moduleBuildLog = nullptr;

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    std::unique_ptr<MockModule> moduleWithPrivateMemory = std::make_unique<MockModule>(device,
                                                                                       moduleBuildLog,
                                                                                       ModuleType::user,
                                                                                       perHwThreadPrivateMemorySizeRequested,
                                                                                       mockKernelImmData.get());
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleWithPrivateMemory->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernelWithPrivateMemory;
    kernelWithPrivateMemory = std::make_unique<ModuleImmutableDataFixture::MockKernel>(moduleWithPrivateMemory.get());

    createKernel(kernelWithPrivateMemory.get());
    EXPECT_NE(nullptr, kernelWithPrivateMemory->privateMemoryGraphicsAllocation);

    size_t sizeContainerWithPrivateMemory = kernelWithPrivateMemory->getInternalResidencyContainer().size();

    perHwThreadPrivateMemorySizeRequested = 0u;
    std::unique_ptr<MockImmutableData> mockKernelImmDataForModuleWithoutPrivateMemory = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    std::unique_ptr<MockModule> moduleWithoutPrivateMemory = std::make_unique<MockModule>(device,
                                                                                          moduleBuildLog,
                                                                                          ModuleType::user,
                                                                                          perHwThreadPrivateMemorySizeRequested,
                                                                                          mockKernelImmDataForModuleWithoutPrivateMemory.get());
    result = moduleWithoutPrivateMemory->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernelWithoutPrivateMemory;
    kernelWithoutPrivateMemory = std::make_unique<ModuleImmutableDataFixture::MockKernel>(moduleWithoutPrivateMemory.get());

    createKernel(kernelWithoutPrivateMemory.get());
    EXPECT_EQ(nullptr, kernelWithoutPrivateMemory->privateMemoryGraphicsAllocation);

    size_t sizeContainerWithoutPrivateMemory = kernelWithoutPrivateMemory->getInternalResidencyContainer().size();

    EXPECT_EQ(sizeContainerWithoutPrivateMemory + 1u, sizeContainerWithPrivateMemory);
}

TEST_F(KernelImmutableDataTests, givenModuleWithPrivateMemoryBiggerThanGlobalMemoryThenPrivateMemoryIsNotAllocated) {
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();
    ModuleBuildLog *moduleBuildLog = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;

    uint32_t perHwThreadPrivateMemorySizeRequested = 0x1000;
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    std::unique_ptr<MockModule> module = std::make_unique<MockModule>(device,
                                                                      moduleBuildLog,
                                                                      ModuleType::user,
                                                                      perHwThreadPrivateMemorySizeRequested,
                                                                      mockKernelImmData.get());
    result = module->initialize(&moduleDesc, device->getNEODevice());
    module->allocatePrivateMemoryPerDispatch = true;
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(module->shouldAllocatePrivateMemoryPerDispatch());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());
    EXPECT_EQ(nullptr, kernel->getPrivateMemoryGraphicsAllocation());
}

HWTEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueThenRayTracingIsInitializedAndPatchedInImplicitArgsBuffer) {
    auto &hwInfo = *neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = false;
    hwInfo.gtSystemInfo.SliceCount = 1;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.SubSliceCount = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 1;
    hwInfo.gtSystemInfo.DualSubSliceCount = 1;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 1;
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    mockDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 4;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    auto result = kernel->initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto rtMemoryBackedBuffer = module->getDevice()->getNEODevice()->getRTMemoryBackedBuffer();
    EXPECT_NE(nullptr, rtMemoryBackedBuffer);

    auto rtDispatchGlobals = neoDevice->getRTDispatchGlobals(NEO::RayTracingHelper::maxBvhLevels);
    EXPECT_NE(nullptr, rtDispatchGlobals);
    auto implicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, implicitArgs);
    EXPECT_EQ_VAL(implicitArgs->v0.rtGlobalBufferPtr, rtDispatchGlobals->rtDispatchGlobalsArray->getGpuAddressToPatch());

    auto &residencyContainer = kernel->getInternalResidencyContainer();

    auto found = std::find(residencyContainer.begin(), residencyContainer.end(), rtMemoryBackedBuffer);
    EXPECT_EQ(residencyContainer.end(), found);

    found = std::find(residencyContainer.begin(), residencyContainer.end(), rtDispatchGlobals->rtDispatchGlobalsArray);
    EXPECT_NE(residencyContainer.end(), found);

    for (auto &rtStack : rtDispatchGlobals->rtStacks) {
        found = std::find(residencyContainer.begin(), residencyContainer.end(), rtStack);
        EXPECT_NE(residencyContainer.end(), found);
    }
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueAndPatchTokenPointerSizeIsZeroThenRayTracingIsInitialized) {
    static_cast<OsAgnosticMemoryManager *>(device->getNEODevice()->getMemoryManager())->turnOnFakingBigAllocations();

    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 0;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    auto result = kernel->initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, module->getDevice()->getNEODevice()->getRTMemoryBackedBuffer());

    auto rtDispatchGlobals = neoDevice->getRTDispatchGlobals(NEO::RayTracingHelper::maxBvhLevels);
    EXPECT_NE(nullptr, rtDispatchGlobals);
}

HWTEST2_F(KernelImmutableDataTests, whenHasRTCallsIsTrueAndNoRTDispatchGlobalsIsAllocatedThenRayTracingIsNotInitialized, IsAtLeastXeCore) {
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 4;

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";
    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    neoDevice->rtDispatchGlobalsForceAllocation = false;

    std::unique_ptr<NEO::MemoryManager> otherMemoryManager;
    otherMemoryManager = std::make_unique<NEO::FailMemoryManager>(0, *neoDevice->executionEnvironment);
    neoDevice->executionEnvironment->memoryManager.swap(otherMemoryManager);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, kernel->initialize(&kernelDesc));

    neoDevice->executionEnvironment->memoryManager.swap(otherMemoryManager);
}

HWTEST2_F(KernelImmutableDataTests, whenHasRTCallsIsTrueAndRTStackAllocationFailsThenRayTracingIsNotInitialized, IsAtLeastXeCore) {
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 4;

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";
    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    neoDevice->rtDispatchGlobalsForceAllocation = false;

    std::unique_ptr<NEO::MemoryManager> otherMemoryManager;
    // Ensure that allocating RTDispatchGlobals succeeds, but first RTStack allocation fails.
    otherMemoryManager = std::make_unique<NEO::FailMemoryManager>(1, *neoDevice->executionEnvironment);
    neoDevice->executionEnvironment->memoryManager.swap(otherMemoryManager);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, kernel->initialize(&kernelDesc));

    neoDevice->executionEnvironment->memoryManager.swap(otherMemoryManager);
}

HWTEST2_F(KernelImmutableDataTests, whenHasRTCallsIsTrueAndRTDispatchGlobalsArrayAllocationSucceedsThenRayTracingIsInitialized, IsPVC) {
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 4;

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";
    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    neoDevice->rtDispatchGlobalsForceAllocation = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->initialize(&kernelDesc));
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsFalseThenRayTracingIsNotInitialized) {
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = false;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->initialize(&kernelDesc));
    EXPECT_FALSE(module->getDevice()->getNEODevice()->rayTracingIsInitialized());
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueThenCrossThreadDataIsPatched) {
    static_cast<OsAgnosticMemoryManager *>(device->getNEODevice()->getMemoryManager())->turnOnFakingBigAllocations();

    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }

    constexpr uint16_t rtGlobalPointerPatchOffset = 8;

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 8;
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.stateless = rtGlobalPointerPatchOffset;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::user,
                                          32u,
                                          mockKernelImmutableData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    auto crossThreadData = std::make_unique<uint32_t[]>(4);
    kernel->crossThreadData.reset(reinterpret_cast<uint8_t *>(crossThreadData.get()));
    kernel->crossThreadDataSize = sizeof(uint32_t[4]);

    auto result = kernel->initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto rtDispatchGlobals = neoDevice->getRTDispatchGlobals(NEO::RayTracingHelper::maxBvhLevels);
    EXPECT_NE(nullptr, rtDispatchGlobals);

    auto dispatchGlobalsAddressPatched = *reinterpret_cast<uint64_t *>(ptrOffset(crossThreadData.get(), rtGlobalPointerPatchOffset));
    auto dispatchGlobalsGpuAddressOffset = static_cast<uint64_t>(rtDispatchGlobals->rtDispatchGlobalsArray->getGpuAddressToPatch());
    EXPECT_EQ(dispatchGlobalsGpuAddressOffset, dispatchGlobalsAddressPatched);

    kernel->crossThreadData.release();
}

using KernelIndirectPropertiesFromIGCTests = KernelImmutableDataTests;

TEST_F(KernelIndirectPropertiesFromIGCTests, givenDetectIndirectAccessInKernelEnabledWhenInitializingKernelWithNoKernelLoadAndNoStoreAndNoAtomicAndNoHasIndirectStatelessAccessThenHasIndirectAccessIsSetToFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    NEO::debugManager.flags.DetectIndirectAccessInKernel.set(1);

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

    kernel->initialize(&desc);

    EXPECT_FALSE(kernel->hasIndirectAccess());
}

TEST_F(KernelIndirectPropertiesFromIGCTests, givenDetectIndirectAccessInKernelEnabledAndPtrPassedByValueWhenInitializingKernelWithNoKernelLoadAndNoStoreAndNoAtomicAndNoHasIndirectStatelessAccessThenHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    NEO::debugManager.flags.DetectIndirectAccessInKernel.set(1);

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    mockKernelImmData->mockKernelDescriptor->kernelAttributes.binaryFormat = NEO::DeviceBinaryFormat::zebin;
    auto ptrByValueArg = ArgDescriptor(ArgDescriptor::argTValue);
    ArgDescValue::Element element{};
    element.isPtr = true;
    ptrByValueArg.as<ArgDescValue>().elements.push_back(element);
    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(ptrByValueArg);
    EXPECT_EQ(mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.size(), 1u);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;

    kernel->initialize(&desc);

    EXPECT_TRUE(kernel->hasIndirectAccess());
}

TEST_F(KernelIndirectPropertiesFromIGCTests, givenDetectIndirectAccessInKernelEnabledAndImplicitArgumentHasIndirectAccessWhenInitializingKernelWithNoKernelLoadAndNoStoreAndNoAtomicAndNoHasIndirectStatelessAccessThenHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    NEO::debugManager.flags.DetectIndirectAccessInKernel.set(1);

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    mockKernelImmData->mockKernelDescriptor->kernelAttributes.binaryFormat = NEO::DeviceBinaryFormat::zebin;
    mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = true;

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;

    kernel->initialize(&desc);

    EXPECT_TRUE(kernel->hasIndirectAccess());
}

TEST_F(KernelIndirectPropertiesFromIGCTests, givenDetectIndirectAccessInKernelEnabledWhenInitializingKernelWithKernelLoadStoreAtomicThenHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    NEO::debugManager.flags.DetectIndirectAccessInKernel.set(1);

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = true;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = true;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = true;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = true;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.flags.useStackCalls = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

        kernel->initialize(&desc);

        EXPECT_FALSE(kernel->hasIndirectAccess());
    }

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.flags.useStackCalls = true;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = false;

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.flags.useStackCalls = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasIndirectAccessInImplicitArg = true;

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }
}

class KernelPropertiesTests : public ModuleFixture, public ::testing::Test {
  public:
    class MockKernel : public KernelImp {
      public:
        using KernelImp::kernelHasIndirectAccess;
        using KernelImp::slmArgsTotalSize;
    };
    void SetUp() override {
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);
        ModuleFixture::setUp();

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();

        ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        kernel = static_cast<MockKernel *>(L0::Kernel::fromHandle(kernelHandle));
        kernel->kernelHasIndirectAccess = true;
    }

    void TearDown() override {
        Kernel::fromHandle(kernelHandle)->destroy();
        ModuleFixture::tearDown();
    }

    ze_kernel_handle_t kernelHandle;
    MockKernel *kernel = nullptr;
    DebugManagerStateRestore restore;
};

TEST_F(KernelPropertiesTests, givenKernelThenCorrectNameIsRetrieved) {
    size_t kernelSize = 0;
    ze_result_t res = kernel->getKernelName(&kernelSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(kernelSize, kernelName.length() + 1);

    size_t alteredKernelSize = kernelSize * 2;
    res = kernel->getKernelName(&alteredKernelSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(alteredKernelSize, kernelSize);

    char *kernelNameRetrieved = new char[kernelSize];
    res = kernel->getKernelName(&kernelSize, kernelNameRetrieved);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(0, strncmp(kernelName.c_str(), kernelNameRetrieved, kernelSize));

    delete[] kernelNameRetrieved;
}

TEST_F(KernelPropertiesTests, givenValidKernelThenPropertiesAreRetrieved) {
    ze_kernel_properties_t kernelProperties = {};

    kernelProperties.requiredNumSubGroups = std::numeric_limits<uint32_t>::max();
    kernelProperties.requiredSubgroupSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.maxSubgroupSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.maxNumSubgroups = std::numeric_limits<uint32_t>::max();
    kernelProperties.localMemSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.privateMemSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.spillMemSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.numKernelArgs = std::numeric_limits<uint32_t>::max();
    memset(&kernelProperties.uuid.kid, std::numeric_limits<int>::max(),
           sizeof(kernelProperties.uuid.kid));
    memset(&kernelProperties.uuid.mid, std::numeric_limits<int>::max(),
           sizeof(kernelProperties.uuid.mid));

    ze_kernel_properties_t kernelPropertiesBefore = {};
    kernelPropertiesBefore = kernelProperties;

    auto expectedSpillSize = 0x100u;
    auto expectedPrivateSize = 0x200u;

    auto &kernelDescriptor = const_cast<KernelDescriptor &>(kernel->getKernelDescriptor());
    kernelDescriptor.kernelAttributes.spillFillScratchMemorySize = expectedSpillSize;
    kernelDescriptor.kernelAttributes.privateScratchMemorySize = expectedPrivateSize;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(6U, kernelProperties.numKernelArgs);

    EXPECT_EQ(0U, kernelProperties.requiredNumSubGroups);
    EXPECT_EQ(0U, kernelProperties.requiredSubgroupSize);

    uint32_t maxSubgroupSize = this->kernel->getKernelDescriptor().kernelAttributes.simdSize;
    ASSERT_NE(0U, maxSubgroupSize);
    EXPECT_EQ(maxSubgroupSize, kernelProperties.maxSubgroupSize);

    uint32_t maxKernelWorkGroupSize = static_cast<uint32_t>(this->module->getMaxGroupSize(this->kernel->getKernelDescriptor()));
    uint32_t maxNumSubgroups = maxKernelWorkGroupSize / maxSubgroupSize;
    EXPECT_EQ(maxNumSubgroups, kernelProperties.maxNumSubgroups);

    EXPECT_EQ(sizeof(float) * 16U, kernelProperties.localMemSize);
    EXPECT_EQ(device->getGfxCoreHelper().getKernelPrivateMemSize(kernelDescriptor), kernelProperties.privateMemSize);
    EXPECT_EQ(expectedSpillSize, kernelProperties.spillMemSize);

    uint8_t zeroKid[ZE_MAX_KERNEL_UUID_SIZE];
    uint8_t zeroMid[ZE_MAX_MODULE_UUID_SIZE];
    memset(&zeroKid, 0, ZE_MAX_KERNEL_UUID_SIZE);
    memset(&zeroMid, 0, ZE_MAX_MODULE_UUID_SIZE);
    EXPECT_EQ(0, memcmp(&kernelProperties.uuid.kid, &zeroKid,
                        sizeof(kernelProperties.uuid.kid)));
    EXPECT_EQ(0, memcmp(&kernelProperties.uuid.mid, &zeroMid,
                        sizeof(kernelProperties.uuid.mid)));
}

HWTEST2_F(KernelPropertiesTests, givenKernelWithPrivateScratchMemoryThenProperPrivateMemorySizeIsReported, IsAtLeastXeCore) {
    ze_kernel_properties_t kernelProperties = {};

    kernelProperties.privateMemSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.spillMemSize = std::numeric_limits<uint32_t>::max();

    auto expectedSpillSize = 0x100u;
    auto expectedPrivateSize = 0x200u;

    auto &kernelDescriptor = const_cast<KernelDescriptor &>(kernel->getKernelDescriptor());
    kernelDescriptor.kernelAttributes.spillFillScratchMemorySize = expectedSpillSize;
    kernelDescriptor.kernelAttributes.privateScratchMemorySize = expectedPrivateSize;
    kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize = 0xDEAD;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(expectedPrivateSize, kernelProperties.privateMemSize);
    EXPECT_EQ(expectedSpillSize, kernelProperties.spillMemSize);
}

TEST_F(KernelPropertiesTests, givenKernelWithInlineAndDynamicSharedLocalMemoryThenTotalLocalMemorySizeIsReported) {
    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.localMemSize = std::numeric_limits<uint32_t>::max();

    uint32_t slmInlineSize = 100u;
    uint32_t slmArgsSize = 4096u;
    uint32_t expectedSlmTotalSize = slmInlineSize + slmArgsSize;

    auto &kernelDescriptor = const_cast<KernelDescriptor &>(kernel->getKernelDescriptor());
    kernelDescriptor.kernelAttributes.slmInlineSize = slmInlineSize;

    kernel->slmArgsTotalSize = slmArgsSize;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(expectedSlmTotalSize, kernelProperties.localMemSize);
}

using KernelMaxNumSubgroupsTests = Test<ModuleImmutableDataFixture>;

HWTEST2_F(KernelMaxNumSubgroupsTests, givenLargeGrfAndSimdSmallerThan32WhenCalculatingMaxWorkGroupSizeThenMaxNumSubgroupsReturnHalfOfDeviceDefault, IsXeCore) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.simdSize = 16;
    kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto mockKernel = std::make_unique<MockKernel>(this->module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    mockKernel->initialize(&kernelDesc);

    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.maxSubgroupSize = std::numeric_limits<uint32_t>::max();
    kernelProperties.maxNumSubgroups = std::numeric_limits<uint32_t>::max();

    ze_kernel_properties_t kernelPropertiesBefore = {};
    kernelPropertiesBefore = kernelProperties;

    ze_result_t res = mockKernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t maxSubgroupSize = mockKernel->getKernelDescriptor().kernelAttributes.simdSize;
    ASSERT_NE(0U, maxSubgroupSize);
    EXPECT_EQ(maxSubgroupSize, kernelProperties.maxSubgroupSize);

    uint32_t maxKernelWorkGroupSize = static_cast<uint32_t>(this->module->getMaxGroupSize(mockKernel->getKernelDescriptor()));
    uint32_t maxNumSubgroups = maxKernelWorkGroupSize / maxSubgroupSize;
    EXPECT_EQ(maxNumSubgroups, kernelProperties.maxNumSubgroups);
    EXPECT_EQ(static_cast<uint32_t>(this->module->getDevice()->getNEODevice()->getDeviceInfo().maxWorkGroupSize) / maxSubgroupSize, maxNumSubgroups * 2);
}

HWTEST2_F(KernelMaxNumSubgroupsTests, givenLargeGrfAndSimdSmallerThan32WhenPassingKernelMaxGroupSizePropertiesStructToGetPropertiesThenHalfDeviceMaxGroupSizeIsReturned, IsXeCore) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.simdSize = 16;
    kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto mockKernel = std::make_unique<MockKernel>(this->module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    mockKernel->initialize(&kernelDesc);

    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;
    ze_kernel_max_group_size_properties_ext_t maxGroupSizeProperties = {};
    maxGroupSizeProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_MAX_GROUP_SIZE_EXT_PROPERTIES;

    kernelProperties.pNext = &maxGroupSizeProperties;

    ze_result_t res = mockKernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(static_cast<uint32_t>(this->module->getDevice()->getNEODevice()->getDeviceInfo().maxWorkGroupSize), maxGroupSizeProperties.maxGroupSize * 2);
}

TEST_F(KernelPropertiesTests, whenPassingKernelMaxGroupSizePropertiesStructToGetPropertiesThenMaxGroupSizeIsReturned) {
    auto &kernelDescriptor = const_cast<KernelDescriptor &>(kernel->getKernelDescriptor());
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    kernelDescriptor.kernelAttributes.simdSize = 16;

    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_kernel_max_group_size_properties_ext_t maxGroupSizeProperties = {};
    maxGroupSizeProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_MAX_GROUP_SIZE_EXT_PROPERTIES;

    kernelProperties.pNext = &maxGroupSizeProperties;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto &device = *module->getDevice();
    auto &gfxCoreHelper = device.getGfxCoreHelper();
    uint32_t maxKernelWorkGroupSize = gfxCoreHelper.adjustMaxWorkGroupSize(kernelDescriptor.kernelAttributes.numGrfRequired, kernelDescriptor.kernelAttributes.simdSize, static_cast<uint32_t>(this->module->getMaxGroupSize(kernelDescriptor)), device.getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(maxKernelWorkGroupSize, maxGroupSizeProperties.maxGroupSize);
}

TEST_F(KernelPropertiesTests, whenPassingPreferredGroupSizeStructToGetPropertiesThenPreferredMultipleIsReturned) {
    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_kernel_preferred_group_size_properties_t preferredGroupProperties = {};
    preferredGroupProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PREFERRED_GROUP_SIZE_PROPERTIES;

    kernelProperties.pNext = &preferredGroupProperties;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &gfxCoreHelper = module->getDevice()->getGfxCoreHelper();
    if (gfxCoreHelper.isFusedEuDispatchEnabled(module->getDevice()->getHwInfo(), false)) {
        EXPECT_EQ(preferredGroupProperties.preferredMultiple, static_cast<uint32_t>(kernel->getImmutableData()->getKernelInfo()->getMaxSimdSize()) * 2);
    } else {
        EXPECT_EQ(preferredGroupProperties.preferredMultiple, static_cast<uint32_t>(kernel->getImmutableData()->getKernelInfo()->getMaxSimdSize()));
    }
}

TEST_F(KernelPropertiesTests, whenPassingPreferredGroupSizeStructWithWrongStypeSuccessIsReturnedAndNoFieldsInPreferredGroupSizeStructAreSet) {
    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_kernel_preferred_group_size_properties_t preferredGroupProperties = {};
    preferredGroupProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;

    kernelProperties.pNext = &preferredGroupProperties;

    uint32_t dummyPreferredMultiple = 101;
    preferredGroupProperties.preferredMultiple = dummyPreferredMultiple;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(preferredGroupProperties.preferredMultiple, dummyPreferredMultiple);
}

TEST_F(KernelPropertiesTests, givenValidKernelThenProfilePropertiesAreRetrieved) {
    zet_profile_properties_t kernelProfileProperties = {};

    kernelProfileProperties.flags = std::numeric_limits<uint32_t>::max();
    kernelProfileProperties.numTokens = std::numeric_limits<uint32_t>::max();

    ze_result_t res = kernel->getProfileInfo(&kernelProfileProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(0U, kernelProfileProperties.flags);
    EXPECT_EQ(0U, kernelProfileProperties.numTokens);
}

TEST_F(KernelPropertiesTests, whenSettingValidKernelIndirectAccessFlagsThenFlagsAreSetCorrectly) {
    UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(false, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectSharedAllocationsAllowed);

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(true, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(true, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(true, unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

TEST_F(KernelPropertiesTests, whenCallingGetIndirectAccessAfterSetIndirectAccessWithDeviceFlagThenCorrectFlagIsReturned) {
    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_kernel_indirect_access_flags_t returnedFlags;
    res = kernel->getIndirectAccess(&returnedFlags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE);
    EXPECT_FALSE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST);
    EXPECT_FALSE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED);
}

TEST_F(KernelPropertiesTests, whenCallingGetIndirectAccessAfterSetIndirectAccessWithHostFlagThenCorrectFlagIsReturned) {
    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_kernel_indirect_access_flags_t returnedFlags;
    res = kernel->getIndirectAccess(&returnedFlags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE);
    EXPECT_TRUE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST);
    EXPECT_FALSE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED);
}

TEST_F(KernelPropertiesTests, whenCallingGetIndirectAccessAfterSetIndirectAccessWithSharedFlagThenCorrectFlagIsReturned) {
    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_kernel_indirect_access_flags_t returnedFlags;
    res = kernel->getIndirectAccess(&returnedFlags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE);
    EXPECT_FALSE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST);
    EXPECT_TRUE(returnedFlags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED);
}
TEST_F(KernelPropertiesTests, givenValidKernelWithIndirectAccessFlagsAndDisableIndirectAccessSetToZeroThenFlagsAreSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);

    UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(false, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectSharedAllocationsAllowed);

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_TRUE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

HWTEST_F(KernelPropertiesTests, whenHasRTCallsIsTrueThenUsesRayTracingIsTrue) {
    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = true;
    WhiteBox<::L0::KernelImmutableData> mockKernelImmutableData = {};

    mockKernelImmutableData.kernelDescriptor = &mockDescriptor;
    mockKernel.kernelImmData = &mockKernelImmutableData;

    EXPECT_TRUE(mockKernel.usesRayTracing());
}

HWTEST_F(KernelPropertiesTests, whenHasRTCallsIsFalseThenUsesRayTracingIsFalse) {
    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    KernelDescriptor mockDescriptor = {};
    mockDescriptor.kernelAttributes.flags.hasRTCalls = false;
    WhiteBox<::L0::KernelImmutableData> mockKernelImmutableData = {};

    mockKernelImmutableData.kernelDescriptor = &mockDescriptor;
    mockKernel.kernelImmData = &mockKernelImmutableData;

    EXPECT_FALSE(mockKernel.usesRayTracing());
}

using KernelIndirectPropertiesTests = KernelPropertiesTests;

TEST_F(KernelIndirectPropertiesTests, whenCallingSetIndirectAccessWithKernelThatHasIndirectAccessThenIndirectAccessIsSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    kernel->kernelHasIndirectAccess = true;

    UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(false, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectSharedAllocationsAllowed);

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_TRUE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

TEST_F(KernelIndirectPropertiesTests, whenCallingSetIndirectAccessWithKernelThatHasIndirectAccessButWithDisableIndirectAccessSetThenIndirectAccessIsNotSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(1);
    kernel->kernelHasIndirectAccess = true;

    UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(false, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectSharedAllocationsAllowed);

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_FALSE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_FALSE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

TEST_F(KernelIndirectPropertiesTests, whenCallingSetIndirectAccessWithKernelThatHasIndirectAccessAndDisableIndirectAccessNotSetThenIndirectAccessIsSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    kernel->kernelHasIndirectAccess = true;

    UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(false, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectSharedAllocationsAllowed);

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_TRUE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

TEST_F(KernelIndirectPropertiesTests, whenCallingSetIndirectAccessWithKernelThatDoesNotHaveIndirectAccessThenIndirectAccessIsSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableIndirectAccess.set(0);
    kernel->kernelHasIndirectAccess = false;

    UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_EQ(false, unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_EQ(false, unifiedMemoryControls.indirectSharedAllocationsAllowed);

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
                                              ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    auto res = kernel->setIndirectAccess(flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    unifiedMemoryControls = kernel->getUnifiedMemoryControls();
    EXPECT_TRUE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

TEST_F(KernelPropertiesTests, givenValidKernelIndirectAccessFlagsSetThenExpectKernelIndirectAllocationsAllowedTrue) {
    EXPECT_EQ(false, kernel->hasIndirectAllocationsAllowed());

    ze_kernel_indirect_access_flags_t flags = ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE;
    auto res = kernel->setIndirectAccess(flags);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(true, kernel->hasIndirectAllocationsAllowed());
}

TEST_F(KernelPropertiesTests, givenValidKernelAndNoMediavfestateThenSpillMemSizeIsZero) {
    ze_kernel_properties_t kernelProperties = {};

    kernelProperties.spillMemSize = std::numeric_limits<uint32_t>::max();

    ze_kernel_properties_t kernelPropertiesBefore = {};
    kernelPropertiesBefore = kernelProperties;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::ModuleImp *moduleImp = reinterpret_cast<L0::ModuleImp *>(module.get());
    NEO::KernelInfo *ki = nullptr;
    for (uint32_t i = 0; i < moduleImp->getTranslationUnit()->programInfo.kernelInfos.size(); i++) {
        ki = moduleImp->getTranslationUnit()->programInfo.kernelInfos[i];
        if (ki->kernelDescriptor.kernelMetadata.kernelName.compare(0, ki->kernelDescriptor.kernelMetadata.kernelName.size(), kernel->getImmutableData()->getDescriptor().kernelMetadata.kernelName) == 0) {
            break;
        }
    }

    EXPECT_EQ(0u, kernelProperties.spillMemSize);
}

TEST_F(KernelPropertiesTests, givenValidKernelAndNollocateStatelessPrivateSurfaceThenPrivateMemSizeIsZero) {
    ze_kernel_properties_t kernelProperties = {};

    kernelProperties.spillMemSize = std::numeric_limits<uint32_t>::max();

    ze_kernel_properties_t kernelPropertiesBefore = {};
    kernelPropertiesBefore = kernelProperties;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::ModuleImp *moduleImp = reinterpret_cast<L0::ModuleImp *>(module.get());
    NEO::KernelInfo *ki = nullptr;
    for (uint32_t i = 0; i < moduleImp->getTranslationUnit()->programInfo.kernelInfos.size(); i++) {
        ki = moduleImp->getTranslationUnit()->programInfo.kernelInfos[i];
        if (ki->kernelDescriptor.kernelMetadata.kernelName.compare(0, ki->kernelDescriptor.kernelMetadata.kernelName.size(), kernel->getImmutableData()->getDescriptor().kernelMetadata.kernelName) == 0) {
            break;
        }
    }

    EXPECT_EQ(0u, kernelProperties.privateMemSize);
}

TEST_F(KernelPropertiesTests, givenValidKernelAndLargeSlmIsSetThenForceLargeSlmIsTrue) {
    EXPECT_EQ(NEO::SlmPolicy::slmPolicyNone, kernel->getSlmPolicy());
    ze_result_t res = kernel->setCacheConfig(ZE_CACHE_CONFIG_FLAG_LARGE_SLM);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(NEO::SlmPolicy::slmPolicyLargeSlm, kernel->getSlmPolicy());
}

TEST_F(KernelPropertiesTests, givenValidKernelAndLargeDataIsSetThenForceLargeDataIsTrue) {
    EXPECT_EQ(NEO::SlmPolicy::slmPolicyNone, kernel->getSlmPolicy());
    ze_result_t res = kernel->setCacheConfig(ZE_CACHE_CONFIG_FLAG_LARGE_DATA);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(NEO::SlmPolicy::slmPolicyLargeData, kernel->getSlmPolicy());
}

struct KernelExtFixture {
    struct MockKernel : public WhiteBox<::L0::KernelImp> {
        using WhiteBox<::L0::KernelImp>::BaseClass::pExtension;
    };
    void setUp() {
        kernel = std::make_unique<MockKernel>();
    }
    void tearDown() {}

    std::unique_ptr<MockKernel> kernel;
    static constexpr uint32_t mclExtType = L0::MCL::MclKernelExt::extensionType;
};

using KernelExtTest = Test<KernelExtFixture>;

TEST_F(KernelExtTest, GivenUnknownExtTypeWhenGettingExtensionThenReturnNullptr) {
    auto ext = kernel->getExtension(0U);
    EXPECT_EQ(nullptr, ext);
}

TEST_F(KernelExtTest, GivenMclExtTypeReturnWhenGettingExtensionThenCreateAndReturnExtension) {
    auto ext = kernel->getExtension(mclExtType);
    EXPECT_NE(nullptr, ext);
    EXPECT_EQ(kernel->pExtension.get(), ext);
}

TEST_F(KernelExtTest, GivenMclExtTypeAndCreatedExtensionWhenGettingExtensionThenReturnExtension) {
    kernel->pExtension = std::make_unique<MCL::MclKernelExt>(0U);
    auto ext = kernel->getExtension(mclExtType);
    EXPECT_EQ(kernel->pExtension.get(), ext);
}

using KernelLocalIdsTest = Test<ModuleFixture>;

TEST_F(KernelLocalIdsTest, WhenKernelIsCreatedThenDefaultLocalIdGenerationbyRuntimeIsTrue) {
    createKernel();

    EXPECT_TRUE(kernel->requiresGenerationOfLocalIdsByRuntime());
}

struct KernelIsaFixture : ModuleFixture {
    void setUp() {
        ModuleFixture::setUp(true);

        auto &capabilityTable = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable;
        bool createBcsEngine = !capabilityTable.blitterOperationsSupported;
        capabilityTable.blitterOperationsSupported = true;

        if (createBcsEngine) {
            auto &engine = device->getNEODevice()->getEngine(0);
            bcsOsContext.reset(OsContext::create(nullptr, device->getNEODevice()->getRootDeviceIndex(), 0,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, device->getNEODevice()->getDeviceBitfield())));
            engine.osContext = bcsOsContext.get();
            engine.commandStreamReceiver->setupContext(*bcsOsContext);
        }
    }

    std::unique_ptr<OsContext> bcsOsContext;
    uint32_t testKernelHeap = 0;
};

using KernelIsaTests = Test<KernelIsaFixture>;

TEST_F(KernelIsaTests, givenKernelAllocationInLocalMemoryWhenCreatingWithoutAllowedCpuAccessThenUseBcsForTransfer) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessDisallowed));
    debugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(NEO::AllocationType::kernelIsa) - 1));
    this->createModuleFromMockBinary(ModuleType::user);

    auto bcsCsr = device->getNEODevice()->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver;
    auto initialTaskCount = bcsCsr->peekTaskCount();

    auto &kernelImmutableData = this->module->kernelImmDatas.back();
    if (kernelImmutableData->getIsaGraphicsAllocation()->isAllocatedInLocalMemoryPool()) {
        EXPECT_EQ(initialTaskCount + 1, bcsCsr->peekTaskCount());
    } else {
        EXPECT_EQ(initialTaskCount, bcsCsr->peekTaskCount());
    }
}

TEST_F(KernelIsaTests, givenKernelAllocationInLocalMemoryWhenCreatingWithAllowedCpuAccessThenDontUseBcsForTransfer) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessAllowed));
    debugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(NEO::AllocationType::kernelIsa) - 1));
    this->createModuleFromMockBinary(ModuleType::user);

    auto bcsCsr = device->getNEODevice()->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver;
    auto initialTaskCount = bcsCsr->peekTaskCount();

    EXPECT_EQ(initialTaskCount, bcsCsr->peekTaskCount());
}

TEST_F(KernelIsaTests, givenKernelAllocationInLocalMemoryWhenCreatingWithDisallowedCpuAccessAndDisabledBlitterThenFallbackToCpuCopy) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessDisallowed));
    debugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(NEO::AllocationType::kernelIsa) - 1));
    this->createModuleFromMockBinary(ModuleType::user);

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;
    auto bcsCsr = device->getNEODevice()->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver;
    auto initialTaskCount = bcsCsr->peekTaskCount();

    EXPECT_EQ(initialTaskCount, bcsCsr->peekTaskCount());
}

TEST_F(KernelIsaTests, givenKernelInfoWhenInitializingImmutableDataWithInternalIsaThenCorrectAllocationTypeIsUsed) {
    this->createModuleFromMockBinary(ModuleType::builtin);

    auto &kernelImmutableData = this->module->kernelImmDatas.back();
    EXPECT_EQ(NEO::AllocationType::kernelIsaInternal, kernelImmutableData->getIsaGraphicsAllocation()->getAllocationType());
}

TEST_F(KernelIsaTests, givenKernelInfoWhenInitializingImmutableDataWithNonInternalIsaThenCorrectAllocationTypeIsUsed) {
    this->createModuleFromMockBinary(ModuleType::user);

    auto &kernelImmutableData = this->module->kernelImmDatas.back();
    EXPECT_EQ(NEO::AllocationType::kernelIsa, kernelImmutableData->getIsaGraphicsAllocation()->getAllocationType());
}

TEST_F(KernelIsaTests, givenKernelInfoWhenInitializingImmutableDataWithIsaThenPaddingIsAdded) {
    this->createModuleFromMockBinary(ModuleType::user);

    auto &kernelImmutableData = this->module->kernelImmDatas.back();
    auto kernelHeapSize = kernelImmutableData->getKernelInfo()->heapInfo.kernelHeapSize;
    auto &helper = device->getNEODevice()->getGfxCoreHelper();
    size_t isaPadding = helper.getPaddingForISAAllocation();
    EXPECT_EQ(kernelImmutableData->getIsaSize(), kernelHeapSize + isaPadding);
}

TEST_F(KernelIsaTests, givenGlobalBuffersWhenCreatingKernelImmutableDataThenBuffersAreAddedToResidencyContainer) {
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1100;
    NEO::MockGraphicsAllocation globalVarBuffer(buffer, gpuAddress, size);
    NEO::MockGraphicsAllocation globalConstBuffer(buffer, gpuAddress, size);

    ModuleBuildLog *moduleBuildLog = nullptr;
    this->module.reset(new WhiteBox<::L0::Module>{this->device, moduleBuildLog, ModuleType::user});
    this->module->mockGlobalVarBuffer = &globalVarBuffer;
    this->module->mockGlobalConstBuffer = &globalConstBuffer;

    this->createModuleFromMockBinary(ModuleType::user);

    for (auto &kernelImmData : this->module->kernelImmDatas) {
        auto &resCont = kernelImmData->getResidencyContainer();
        EXPECT_EQ(1, std::count(resCont.begin(), resCont.end(), &globalVarBuffer));
        EXPECT_EQ(1, std::count(resCont.begin(), resCont.end(), &globalConstBuffer));
    }
    this->module->translationUnit->globalConstBuffer = nullptr;
    this->module->translationUnit->globalVarBuffer = nullptr;
}

using KernelImpPatchBindlessTest = Test<ModuleFixture>;

TEST_F(KernelImpPatchBindlessTest, GivenKernelImpWhenPatchBindlessOffsetCalledThenOffsetPatchedCorrectly) {
    Mock<KernelImp> kernel;
    neoDevice->incRefInternal();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);
    Mock<Module> mockModule(device, nullptr);
    kernel.module = &mockModule;
    NEO::MockGraphicsAllocation alloc;
    uint32_t bindless = 0x40;
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t size = gfxCoreHelper.getRenderSurfaceStateSize();
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &alloc, NEO::BindlessHeapsHelper::globalSsh);
    alloc.setBindlessInfo(expectedSsInHeap);

    auto patchLocation = ptrOffset(kernel.getCrossThreadData(), bindless);
    auto patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(expectedSsInHeap.surfaceStateOffset));

    auto ssPtr = kernel.patchBindlessSurfaceState(&alloc, bindless);

    EXPECT_EQ(ssPtr, expectedSsInHeap.ssPtr);
    EXPECT_TRUE(memcmp(const_cast<uint8_t *>(patchLocation), &patchValue, sizeof(patchValue)) == 0);
    EXPECT_FALSE(std::find(kernel.getArgumentsResidencyContainer().begin(), kernel.getArgumentsResidencyContainer().end(), expectedSsInHeap.heapAllocation) != kernel.getArgumentsResidencyContainer().end());
    neoDevice->decRefInternal();
}

HWTEST_F(KernelImpPatchBindlessTest, GivenBindlessKernelAndNoGlobalBindlessAllocatorWhenInitializedThenBindlessOffsetSetAndUsingSurfaceStateAreFalse) {
    ModuleBuildLog *moduleBuildLog = nullptr;
    this->module.reset(new WhiteBox<::L0::Module>{this->device, moduleBuildLog, ModuleType::user});
    this->createModuleFromMockBinary(ModuleType::user);

    for (auto &kernelImmData : this->module->kernelImmDatas) {
        auto &arg = const_cast<NEO::ArgDescPointer &>(kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
        arg.bindless = 0x40;
        arg.bindful = undefined<SurfaceStateHeapOffset>;
        const_cast<NEO::KernelDescriptor &>(kernelImmData->getDescriptor()).kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
        const_cast<NEO::KernelDescriptor &>(kernelImmData->getDescriptor()).kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;
    }

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    EXPECT_FALSE(mockKernel.isBindlessOffsetSet[0]);
    EXPECT_FALSE(mockKernel.usingSurfaceStateHeap[0]);
}

HWTEST_F(KernelImpPatchBindlessTest, GivenKernelImpWhenSetSurfaceStateBindlessThenSurfaceStateUpdated) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);
    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = 0x40;
    arg.bindful = undefined<SurfaceStateHeapOffset>;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t size = gfxCoreHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::globalSsh);
    mockAllocation.setBindlessInfo(expectedSsInHeap);

    memset(expectedSsInHeap.ssPtr, 0, size);
    auto surfaceStateBefore = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);
    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);

    auto surfaceStateAfter = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);

    EXPECT_FALSE(memcmp(&surfaceStateAfter, &surfaceStateBefore, size) == 0);
    EXPECT_TRUE(mockKernel.isBindlessOffsetSet[0]);
    EXPECT_FALSE(mockKernel.usingSurfaceStateHeap[0]);
}

HWTEST_F(KernelImpPatchBindlessTest, GivenMisalignedBufferAddressWhenSettingSurfaceStateThenSurfaceStateInKernelHeapIsUsed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);
    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = 0x40;
    arg.bindful = undefined<SurfaceStateHeapOffset>;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).initBindlessOffsetToSurfaceState();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t size = gfxCoreHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::globalSsh);
    mockAllocation.setBindlessInfo(expectedSsInHeap);

    memset(expectedSsInHeap.ssPtr, 0, size);

    EXPECT_EQ(0u, mockKernel.getSurfaceStateHeapDataSize());
    EXPECT_FALSE(mockKernel.isBindlessOffsetSet[0]);
    EXPECT_FALSE(mockKernel.usingSurfaceStateHeap[0]);

    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);
    auto surfaceStateBefore = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);

    mockKernel.setBufferSurfaceState(0, ptrOffset(buffer, 8), &mockAllocation);
    auto surfaceStateAfter = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);
    auto surfaceStateOnSsh = *reinterpret_cast<RENDER_SURFACE_STATE *>(mockKernel.surfaceStateHeapData.get());

    EXPECT_TRUE(memcmp(&surfaceStateAfter, &surfaceStateBefore, size) == 0);

    EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(buffer, 8)), surfaceStateOnSsh.getSurfaceBaseAddress());
    EXPECT_FALSE(mockKernel.isBindlessOffsetSet[0]);
    EXPECT_TRUE(mockKernel.usingSurfaceStateHeap[0]);
    EXPECT_EQ(mockKernel.surfaceStateHeapDataSize, mockKernel.getSurfaceStateHeapDataSize());
}

HWTEST_F(KernelImpPatchBindlessTest, GivenMisalignedAndAlignedBufferAddressWhenSettingSurfaceStateThenKernelReportsNonZeroSurfaceStateHeapDataSize) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);
    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = 0x40;
    arg.bindful = undefined<SurfaceStateHeapOffset>;
    auto &arg2 = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[1].template as<NEO::ArgDescPointer>());
    arg2.bindless = 0x48;
    arg2.bindful = undefined<SurfaceStateHeapOffset>;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;
    const_cast<NEO::KernelDescriptor &>(mockKernel.kernelImmData->getDescriptor()).initBindlessOffsetToSurfaceState();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t size = gfxCoreHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::globalSsh);
    mockAllocation.setBindlessInfo(expectedSsInHeap);

    memset(expectedSsInHeap.ssPtr, 0, size);

    // misaligned buffer - requires different surface state
    mockKernel.setBufferSurfaceState(0, ptrOffset(buffer, 8), &mockAllocation);
    // aligned buffer - using allocated bindless surface state
    mockKernel.setBufferSurfaceState(1, buffer, &mockAllocation);

    auto surfaceStateOnSsh = reinterpret_cast<RENDER_SURFACE_STATE *>(mockKernel.surfaceStateHeapData.get());

    EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(buffer, 8)), surfaceStateOnSsh->getSurfaceBaseAddress());
    EXPECT_FALSE(mockKernel.isBindlessOffsetSet[0]);
    EXPECT_TRUE(mockKernel.usingSurfaceStateHeap[0]);

    EXPECT_TRUE(mockKernel.isBindlessOffsetSet[1]);
    EXPECT_FALSE(mockKernel.usingSurfaceStateHeap[1]);

    EXPECT_EQ(mockKernel.surfaceStateHeapDataSize, mockKernel.getSurfaceStateHeapDataSize());
}

HWTEST_F(KernelImpPatchBindlessTest, GivenKernelImpWhenSetSurfaceStateBindfulThenSurfaceStateNotUpdated) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = 0x40;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t size = gfxCoreHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::globalSsh);
    mockAllocation.setBindlessInfo(expectedSsInHeap);

    memset(expectedSsInHeap.ssPtr, 0, size);
    auto surfaceStateBefore = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);
    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);

    auto surfaceStateAfter = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);

    EXPECT_TRUE(memcmp(&surfaceStateAfter, &surfaceStateBefore, size) == 0);
}

using KernelImpL3CachingTests = Test<ModuleFixture>;

HWTEST_F(KernelImpL3CachingTests, GivenKernelImpWhenSetSurfaceStateWithUnalignedMemoryThenL3CachingIsDisabled) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = 0x40;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    size_t size = gfxCoreHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(0x20123);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::globalSsh);
    mockAllocation.setBindlessInfo(expectedSsInHeap);

    memset(expectedSsInHeap.ssPtr, 0, size);
    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);
    EXPECT_EQ(mockKernel.getKernelRequiresQueueUncachedMocs(), true);
}

struct MyMockKernel : public Mock<KernelImp> {
    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
        setSurfaceStateCalled = true;
    }
    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *peerAllocData) override {
        return KernelImp::setArgBufferWithAlloc(argIndex, argVal, allocation, peerAllocData);
    }
    bool setSurfaceStateCalled = false;
};

TEST_F(KernelImpPatchBindlessTest, GivenValidBindlessOffsetWhenSetArgBufferWithAllocThensetBufferSurfaceStateCalled) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = 0x40;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    NEO::MockGraphicsAllocation alloc;

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc, nullptr);

    EXPECT_TRUE(mockKernel.setSurfaceStateCalled);
}

TEST_F(KernelImpPatchBindlessTest, GivenValidBindfulOffsetWhenSetArgBufferWithAllocThensetBufferSurfaceStateCalled) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = 0x40;

    NEO::MockGraphicsAllocation alloc;

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc, nullptr);

    EXPECT_TRUE(mockKernel.setSurfaceStateCalled);
}

TEST_F(KernelImpPatchBindlessTest, GivenUndefiedBidfulAndBindlesstOffsetWhenSetArgBufferWithAllocThenSetBufferSurfaceStateIsNotCalled) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    NEO::MockGraphicsAllocation alloc;

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc, nullptr);

    EXPECT_FALSE(mockKernel.setSurfaceStateCalled);
}

using KernelBindlessUncachedMemoryTests = Test<ModuleFixture>;

TEST_F(KernelBindlessUncachedMemoryTests, givenBindlessKernelAndAllocDataNoTfoundThenKernelRequiresUncachedMocsIsSet) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    NEO::MockGraphicsAllocation alloc;

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc, nullptr);
    EXPECT_FALSE(mockKernel.getKernelRequiresUncachedMocs());
}

TEST_F(KernelBindlessUncachedMemoryTests,
       givenNonUncachedAllocationSetAsArgumentFollowedByNonUncachedAllocationThenRequiresUncachedMocsIsCorrectlySet) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    {
        void *devicePtr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                                  &deviceDesc,
                                                  16384u,
                                                  0u,
                                                  &devicePtr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_FALSE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(devicePtr);
    }

    {
        void *devicePtr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                                  &deviceDesc,
                                                  16384u,
                                                  0u,
                                                  &devicePtr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_FALSE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(devicePtr);
    }
}

TEST_F(KernelBindlessUncachedMemoryTests,
       givenUncachedAllocationSetAsArgumentFollowedByUncachedAllocationThenRequiresUncachedMocsIsCorrectlySet) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    {
        void *devicePtr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                                  &deviceDesc,
                                                  16384u,
                                                  0u,
                                                  &devicePtr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_TRUE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(devicePtr);
    }

    {
        void *devicePtr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                                  &deviceDesc,
                                                  16384u,
                                                  0u,
                                                  &devicePtr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_TRUE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(devicePtr);
    }
}

TEST_F(KernelBindlessUncachedMemoryTests,
       givenUncachedAllocationSetAsArgumentFollowedByNonUncachedAllocationThenRequiresUncachedMocsIsCorrectlySet) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    {
        void *devicePtr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                                  &deviceDesc,
                                                  16384u,
                                                  0u,
                                                  &devicePtr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_TRUE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(devicePtr);
    }

    {
        void *devicePtr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                                  &deviceDesc,
                                                  16384u,
                                                  0u,
                                                  &devicePtr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_FALSE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(devicePtr);
    }
}

TEST_F(KernelBindlessUncachedMemoryTests,
       givenUncachedHostAllocationSetAsArgumentFollowedByNonUncachedHostAllocationThenRequiresUncachedMocsIsCorrectlySet) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    MyMockKernel mockKernel;

    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    {
        void *ptr = nullptr;
        ze_host_mem_alloc_desc_t hostDesc = {};
        hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        ze_result_t res = context->allocHostMem(&hostDesc,
                                                16384u,
                                                0u,
                                                &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_TRUE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(ptr);
    }

    {
        void *ptr = nullptr;
        ze_host_mem_alloc_desc_t hostDesc = {};
        ze_result_t res = context->allocHostMem(&hostDesc,
                                                16384u,
                                                0u,
                                                &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        auto alloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        EXPECT_NE(nullptr, alloc);

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc, nullptr);
        EXPECT_FALSE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(ptr);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct MyMockImage : public WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>> {
    void copySurfaceStateToSSH(void *surfaceStateHeap,
                               uint32_t surfaceStateOffset,
                               uint32_t bindlessSlot,
                               bool isMediaBlockArg) override {

        switch (bindlessSlot) {
        case NEO::BindlessImageSlot::implicitArgs:
            passedImplicitArgsSurfaceStateHeap = surfaceStateHeap;
            passedImplicitArgsSurfaceStateOffset = surfaceStateOffset;
            break;
        case BindlessImageSlot::redescribedImage:
            passedRedescribedSurfaceStateHeap = surfaceStateHeap;
            passedRedescribedSurfaceStateOffset = surfaceStateOffset;
            break;
        case BindlessImageSlot::packedImage:
        case BindlessImageSlot::image:
        default:
            passedSurfaceStateHeap = surfaceStateHeap;
            passedSurfaceStateOffset = surfaceStateOffset;
            break;
        }
    }

    void *passedSurfaceStateHeap = nullptr;
    uint32_t passedSurfaceStateOffset = 0;

    void *passedImplicitArgsSurfaceStateHeap = nullptr;
    uint32_t passedImplicitArgsSurfaceStateOffset = 0;

    void *passedRedescribedSurfaceStateHeap = nullptr;
    uint32_t passedRedescribedSurfaceStateOffset = 0;

    void *passedPackedSurfaceStateHeap = nullptr;
    uint32_t passedPackedSurfaceStateOffset = 0;
};

HWTEST2_F(SetKernelArg, givenImageAndBindlessKernelWhenSetArgImageThenCopySurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    createKernel();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);
    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = kernel->setArgImage(3, sizeof(imageHW.get()), &handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &expectedSsInHeap = imageHW->getAllocation()->getBindlessInfo();
    EXPECT_EQ(imageHW->passedSurfaceStateHeap, expectedSsInHeap.ssPtr);
    EXPECT_EQ(imageHW->passedSurfaceStateOffset, 0u);
    EXPECT_TRUE(kernel->isBindlessOffsetSet[3]);
    EXPECT_FALSE(kernel->usingSurfaceStateHeap[3]);
    EXPECT_EQ(0, std::count(kernel->argumentsResidencyContainer.begin(), kernel->argumentsResidencyContainer.end(), expectedSsInHeap.heapAllocation));
}

HWTEST2_F(SetKernelArg, givenNoGlobalAllocatorAndBindlessKernelWhenSetArgImageThenBindlessOffsetIsNotSetAndSshIsUsed, ImageSupport) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = kernel->setArgImage(3, sizeof(imageHW.get()), &handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(kernel->isBindlessOffsetSet[3]);
    EXPECT_TRUE(kernel->usingSurfaceStateHeap[3]);
}

HWTEST2_F(SetKernelArg, givenBindlessKernelAndNoAvailableSpaceOnSshWhenSetArgImageCalledThenOutOfMemoryErrorReturned, ImageSupport) {
    createKernel();

    auto mockMemManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    mockMemManager->failInDevicePool = true;
    mockMemManager->failAllocateSystemMemory = true;
    mockMemManager->failAllocate32Bit = true;
    bindlessHelper->globalSsh->getSpace(bindlessHelper->globalSsh->getAvailableSpace());

    ret = kernel->setArgImage(3, sizeof(imageHW.get()), &handle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, ret);

    auto &bindlessInfo = imageHW->getAllocation()->getBindlessInfo();
    EXPECT_EQ(nullptr, bindlessInfo.ssPtr);
    EXPECT_EQ(nullptr, bindlessInfo.heapAllocation);
}

HWTEST2_F(SetKernelArg, givenImageAndBindlessKernelWhenSetArgImageThenCopyImplicitArgsSurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    createKernel();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);
    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = kernel->setArgImage(3, sizeof(imageHW.get()), &handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto &expectedSsInHeap = imageHW->getAllocation()->getBindlessInfo();
    EXPECT_EQ(imageHW->passedImplicitArgsSurfaceStateHeap, ptrOffset(expectedSsInHeap.ssPtr, surfaceStateSize));
    EXPECT_EQ(imageHW->passedImplicitArgsSurfaceStateOffset, 0u);
    EXPECT_TRUE(kernel->isBindlessOffsetSet[3]);
    EXPECT_FALSE(kernel->usingSurfaceStateHeap[3]);
}

HWTEST2_F(SetKernelArg, givenImageBindlessKernelAndGlobalBindlessHelperWhenSetArgRedescribedImageCalledThenCopySurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    createKernel();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);
    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = kernel->setArgRedescribedImage(3, handle, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto &expectedSsInHeap = imageHW->getAllocation()->getBindlessInfo();
    EXPECT_EQ(imageHW->passedRedescribedSurfaceStateHeap, ptrOffset(expectedSsInHeap.ssPtr, surfaceStateSize * NEO::BindlessImageSlot::redescribedImage));
    EXPECT_EQ(imageHW->passedRedescribedSurfaceStateOffset, 0u);
    EXPECT_TRUE(kernel->isBindlessOffsetSet[3]);
    EXPECT_FALSE(kernel->usingSurfaceStateHeap[3]);
    EXPECT_EQ(0, std::count(kernel->argumentsResidencyContainer.begin(), kernel->argumentsResidencyContainer.end(), expectedSsInHeap.heapAllocation));
}

HWTEST2_F(SetKernelArg, givenHeaplessWhenPatchingImageWithBindlessEnabledCorrectSurfaceStateAddressIsPatchedInCrossThreadData, ImageSupport) {

    for (auto heaplessEnabled : {false, true}) {

        createKernel();
        kernel->heaplessEnabled = heaplessEnabled;

        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                                 neoDevice->getNumGenericSubDevices() > 1);
        NEO::BindlessHeapsHelper *bindlessHeapsHelper = neoDevice->getBindlessHeapsHelper();
        ASSERT_NE(nullptr, bindlessHeapsHelper);

        auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
        auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
        const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
        imageArg.bindless = 0x8;
        imageArg.bindful = undefined<SurfaceStateHeapOffset>;
        ze_image_desc_t desc = {};
        desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

        auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
        auto ret = imageHW->initialize(device, &desc);
        auto handle = imageHW->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

        ret = kernel->setArgRedescribedImage(3, handle, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

        auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
        auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

        auto ctd = kernel->crossThreadData.get();

        auto ssInHeap = imageHW->getBindlessSlot();
        auto patchLocation = ptrOffset(ctd, imageArg.bindless);
        uint64_t bindlessSlotOffset = ssInHeap->surfaceStateOffset + surfaceStateSize * NEO::BindlessImageSlot::redescribedImage;
        uint64_t expectedPatchValue = kernel->heaplessEnabled
                                          ? bindlessSlotOffset
                                          : gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(bindlessSlotOffset));

        if (kernel->heaplessEnabled) {
            uint64_t patchedValued = *(reinterpret_cast<uint64_t *>(patchLocation));
            EXPECT_EQ(expectedPatchValue, patchedValued);
        } else {
            uint32_t patchedValued = *(reinterpret_cast<uint32_t *>(patchLocation));
            EXPECT_EQ(static_cast<uint32_t>(expectedPatchValue), patchedValued);
        }
    }
}

HWTEST2_F(SetKernelArg, givenGlobalBindlessHelperAndImageViewWhenAllocatingBindlessSlotThenViewHasDifferentSlotThanParentImage, ImageSupport) {
    createKernel();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);
    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(ZE_RESULT_SUCCESS, imageHW->allocateBindlessSlot());

    ze_image_handle_t imageViewHandle;
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC;
    ret = imageHW->createView(device, &desc, &imageViewHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imageView = Image::fromHandle(imageViewHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, imageView->allocateBindlessSlot());
    auto ssInHeap = imageHW->getBindlessSlot();
    auto ssInHeapView = imageView->getBindlessSlot();

    ASSERT_NE(nullptr, ssInHeap);
    ASSERT_NE(nullptr, ssInHeapView);

    EXPECT_NE(ssInHeap->surfaceStateOffset, ssInHeapView->surfaceStateOffset);

    // calling allocateBindlessSlot again should not change slot
    EXPECT_EQ(ZE_RESULT_SUCCESS, imageView->allocateBindlessSlot());
    auto ssInHeapView2 = imageView->getBindlessSlot();
    EXPECT_EQ(ssInHeapView->surfaceStateOffset, ssInHeapView2->surfaceStateOffset);

    imageView->destroy();
}

HWTEST2_F(SetKernelArg, givenGlobalBindlessHelperImageViewAndNoAvailableSpaceOnSshWhenAllocatingBindlessSlotThenOutOfMemoryErrorReturned, ImageSupport) {
    createKernel();
    auto mockMemManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    mockMemManager->failInDevicePool = true;
    mockMemManager->failAllocateSystemMemory = true;
    mockMemManager->failAllocate32Bit = true;
    bindlessHelper->globalSsh->getSpace(bindlessHelper->globalSsh->getAvailableSpace());

    ze_image_handle_t imageViewHandle;
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC;
    ret = imageHW->createView(device, &desc, &imageViewHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imageView = Image::fromHandle(imageViewHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, imageView->allocateBindlessSlot());
    auto ssInHeap = imageHW->getBindlessSlot();
    auto ssInHeapView = imageView->getBindlessSlot();

    EXPECT_EQ(nullptr, ssInHeap);
    EXPECT_EQ(nullptr, ssInHeapView);
    imageView->destroy();
}

HWTEST2_F(SetKernelArg, givenNoGlobalBindlessHelperAndImageViewWhenAllocatingBindlessSlotThenSlotIsNotAllocated, ImageSupport) {
    createKernel();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset();
    ASSERT_EQ(nullptr, neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get());

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(ZE_RESULT_SUCCESS, imageHW->allocateBindlessSlot());

    ze_image_handle_t imageViewHandle;
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC;
    ret = imageHW->createView(device, &desc, &imageViewHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto imageView = Image::fromHandle(imageViewHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, imageView->allocateBindlessSlot());
    auto ssInHeap = imageHW->getBindlessSlot();
    auto ssInHeapView = imageView->getBindlessSlot();

    ASSERT_EQ(nullptr, ssInHeap);
    ASSERT_EQ(nullptr, ssInHeapView);

    imageView->destroy();
}

HWTEST2_F(SetKernelArg, givenImageAndBindlessKernelWhenSetArgRedescribedImageCalledThenCopySurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptor.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptor.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescImage>().bindless = 0x0;
    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(4 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 4 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    mockKernel.surfaceStateHeapData = std::make_unique<uint8_t[]>(surfaceStateSize);
    mockKernel.descriptor.initBindlessOffsetToSurfaceState();
    mockKernel.argumentsResidencyContainer.resize(1);
    mockKernel.isBindlessOffsetSet.resize(1, 0);
    mockKernel.usingSurfaceStateHeap.resize(1, false);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = mockKernel.setArgRedescribedImage(0, handle, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    void *expectedSsInHeap = nullptr;
    if (bindlessHeapsHelper) {
        expectedSsInHeap = ptrOffset(imageHW->getBindlessSlot()->ssPtr, surfaceStateSize * NEO::BindlessImageSlot::redescribedImage);
    } else {
        expectedSsInHeap = ptrOffset(mockKernel.surfaceStateHeapData.get(), mockKernel.kernelImmData->getDescriptor().getBindlessOffsetToSurfaceState().find(0x0)->second * surfaceStateSize);
    }

    EXPECT_EQ(imageHW->passedRedescribedSurfaceStateHeap, expectedSsInHeap);
    EXPECT_EQ(imageHW->passedRedescribedSurfaceStateOffset, 0u);

    if (bindlessHeapsHelper) {
        EXPECT_TRUE(mockKernel.isBindlessOffsetSet[0]);
    } else {
        EXPECT_TRUE(mockKernel.usingSurfaceStateHeap[0]);
    }
}

HWTEST2_F(SetKernelArg, givenBindlessKernelAndNoAvailableSpaceOnSshWhenSetArgRedescribedImageCalledThenOutOfMemoryErrorReturned, ImageSupport) {
    createKernel();

    auto mockMemManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    mockMemManager->failInDevicePool = true;
    mockMemManager->failAllocateSystemMemory = true;
    mockMemManager->failAllocate32Bit = true;
    bindlessHelper->globalSsh->getSpace(bindlessHelper->globalSsh->getAvailableSpace());

    ret = kernel->setArgRedescribedImage(3, handle, false);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, ret);

    auto &bindlessInfo = imageHW->getAllocation()->getBindlessInfo();
    EXPECT_EQ(nullptr, bindlessInfo.ssPtr);
    EXPECT_EQ(nullptr, bindlessInfo.heapAllocation);
}

HWTEST_F(SetKernelArg, givenBindlessKernelAndNoAvailableSpaceOnSshWhenSetArgBufferCalledThenOutOfMemoryErrorReturned) {

    auto mockMemManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    auto bindlessHelper = new MockBindlesHeapsHelper(neoDevice,
                                                     neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);
    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = 0x40;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    bindlessHelper->globalSsh->getSpace(bindlessHelper->globalSsh->getAvailableSpace());

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto allocationProperties = NEO::SVMAllocsManager::SvmAllocationProperties{};
    auto svmAllocation = svmAllocsManager->createSVMAlloc(4096, allocationProperties, context->rootDeviceIndices, context->deviceBitfields);

    mockMemManager->failInDevicePool = true;
    mockMemManager->failAllocateSystemMemory = true;
    mockMemManager->failAllocate32Bit = true;
    auto ret = mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, ret);

    svmAllocsManager->freeSVMAlloc(svmAllocation);
}

HWTEST_F(SetKernelArg, givenSlmPointerWhenSettingKernelArgThenPropertyIsSaved) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    WhiteBoxKernelHw<FamilyType::gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    {
        auto &baseArg = const_cast<NEO::ArgDescriptor &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0]);
        ArgTypeTraits &traits = const_cast<NEO::ArgTypeTraits &>(baseArg.getTraits());
        traits.addressQualifier = static_cast<uint8_t>(NEO::KernelArgMetadata::AddrLocal);

        auto &arg = const_cast<NEO::ArgDescPointer &>(baseArg.template as<NEO::ArgDescPointer>());
        arg.stateless = undefined<SurfaceStateHeapOffset>;
        arg.bindless = undefined<SurfaceStateHeapOffset>;
        arg.bindful = undefined<SurfaceStateHeapOffset>;
        arg.requiredSlmAlignment = 4;
        arg.slmOffset = 0x20;
    }

    {
        auto &baseArg = mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[1];
        ArgTypeTraits &traits = const_cast<NEO::ArgTypeTraits &>(baseArg.getTraits());
        traits.addressQualifier = static_cast<uint8_t>(NEO::KernelArgMetadata::AddrLocal);

        auto &arg = const_cast<NEO::ArgDescPointer &>(baseArg.template as<NEO::ArgDescPointer>());
        arg.stateless = undefined<SurfaceStateHeapOffset>;
        arg.bindless = undefined<SurfaceStateHeapOffset>;
        arg.bindful = undefined<SurfaceStateHeapOffset>;
        arg.requiredSlmAlignment = 8;
        arg.slmOffset = 0x40;
    }

    auto ret = mockKernel.setArgBuffer(0, 64, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = mockKernel.setArgBuffer(1, 32, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto slmArgOffsetValues = mockKernel.getSlmArgOffsetValues();
    EXPECT_EQ(0u, slmArgOffsetValues[0]);
    EXPECT_EQ(64u, slmArgOffsetValues[1]);

    auto slmArgSizes = mockKernel.getSlmArgSizes();
    EXPECT_EQ(64u, slmArgSizes[0]);
    EXPECT_EQ(32u, slmArgSizes[1]);

    auto argSlmAlignment = mockKernel.getRequiredSlmAlignment(0);
    EXPECT_EQ(4u, argSlmAlignment);

    argSlmAlignment = mockKernel.getRequiredSlmAlignment(1);
    EXPECT_EQ(8u, argSlmAlignment);
}

HWTEST2_F(SetKernelArg, givenImageAndBindfulKernelWhenSetArgImageThenCopySurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    imageArg.bindless = undefined<CrossThreadDataOffset>;
    imageArg.bindful = 0x40;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<FamilyType::gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    kernel->setArgImage(3, sizeof(imageHW.get()), &handle);

    EXPECT_EQ(imageHW->passedSurfaceStateHeap, kernel->getSurfaceStateHeapData());
    EXPECT_EQ(imageHW->passedSurfaceStateOffset, imageArg.bindful);
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct MyMockImageMediaBlock : public WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>> {
    void copySurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset, uint32_t bindlessSlot, bool isMediaBlockArg) override {
        isMediaBlockPassedValue = isMediaBlockArg;
    }
    bool isMediaBlockPassedValue = false;
};

HWTEST2_F(SetKernelArg, givenSupportsMediaBlockAndIsMediaBlockImageWhenSetArgImageIsCalledThenIsMediaBlockArgIsPassedCorrectly, ImageSupport) {
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    createKernel();
    auto argIndex = 3u;
    auto &arg = const_cast<NEO::ArgDescriptor &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex]);
    auto imageHW = std::make_unique<MyMockImageMediaBlock<FamilyType::gfxCoreFamily>>();
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto handle = imageHW->toHandle();

    {
        hwInfo->capabilityTable.supportsMediaBlock = true;
        arg.getExtendedTypeInfo().isMediaBlockImage = true;
        kernel->setArgImage(argIndex, sizeof(imageHW.get()), &handle);
        EXPECT_TRUE(imageHW->isMediaBlockPassedValue);
    }
    {
        hwInfo->capabilityTable.supportsMediaBlock = false;
        arg.getExtendedTypeInfo().isMediaBlockImage = true;
        kernel->setArgImage(argIndex, sizeof(imageHW.get()), &handle);
        EXPECT_FALSE(imageHW->isMediaBlockPassedValue);
    }
    {
        hwInfo->capabilityTable.supportsMediaBlock = true;
        arg.getExtendedTypeInfo().isMediaBlockImage = false;
        kernel->setArgImage(argIndex, sizeof(imageHW.get()), &handle);
        EXPECT_FALSE(imageHW->isMediaBlockPassedValue);
    }
    {
        hwInfo->capabilityTable.supportsMediaBlock = false;
        arg.getExtendedTypeInfo().isMediaBlockImage = false;
        kernel->setArgImage(argIndex, sizeof(imageHW.get()), &handle);
        EXPECT_FALSE(imageHW->isMediaBlockPassedValue);
    }
}

using ImportHostPointerSetKernelArg = Test<ImportHostPointerModuleFixture>;
TEST_F(ImportHostPointerSetKernelArg, givenHostPointerImportedWhenSettingKernelArgThenUseHostPointerAllocation) {
    createKernel();

    auto ret = driverHandle->importExternalPointer(hostPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = kernel->setArgBuffer(0, sizeof(hostPointer), &hostPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = driverHandle->releaseImportedPointer(hostPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

class KernelGlobalWorkOffsetTests : public ModuleFixture, public ::testing::Test {
  public:
    void SetUp() override {
        ModuleFixture::setUp();

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();

        ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        kernel = L0::Kernel::fromHandle(kernelHandle);
    }

    void TearDown() override {
        Kernel::fromHandle(kernelHandle)->destroy();
        ModuleFixture::tearDown();
    }

    ze_kernel_handle_t kernelHandle;
    L0::Kernel *kernel = nullptr;
};

TEST_F(KernelGlobalWorkOffsetTests, givenCallToSetGlobalWorkOffsetThenOffsetsAreSet) {
    uint32_t globalOffsetx = 10;
    uint32_t globalOffsety = 20;
    uint32_t globalOffsetz = 30;

    ze_result_t res = kernel->setGlobalOffsetExp(globalOffsetx, globalOffsety, globalOffsetz);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    EXPECT_EQ(globalOffsetx, kernelImp->getGlobalOffsets()[0]);
    EXPECT_EQ(globalOffsety, kernelImp->getGlobalOffsets()[1]);
    EXPECT_EQ(globalOffsetz, kernelImp->getGlobalOffsets()[2]);
}

TEST_F(KernelGlobalWorkOffsetTests, whenSettingGlobalOffsetThenCrossThreadDataIsPatched) {
    uint32_t globalOffsetx = 10;
    uint32_t globalOffsety = 20;
    uint32_t globalOffsetz = 30;

    ze_result_t res = kernel->setGlobalOffsetExp(globalOffsetx, globalOffsety, globalOffsetz);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    kernelImp->patchGlobalOffset();

    const NEO::KernelDescriptor &desc = kernelImp->getImmutableData()->getDescriptor();
    auto dst = ArrayRef<const uint8_t>(kernelImp->getCrossThreadData(), kernelImp->getCrossThreadDataSize());
    EXPECT_EQ(*(dst.begin() + desc.payloadMappings.dispatchTraits.globalWorkOffset[0]), globalOffsetx);
    EXPECT_EQ(*(dst.begin() + desc.payloadMappings.dispatchTraits.globalWorkOffset[1]), globalOffsety);
    EXPECT_EQ(*(dst.begin() + desc.payloadMappings.dispatchTraits.globalWorkOffset[2]), globalOffsetz);
}

class KernelProgramBinaryTests : public ModuleFixture, public ::testing::Test {
  public:
    void SetUp() override {
        ModuleFixture::setUp();

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();

        ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        kernel = L0::Kernel::fromHandle(kernelHandle);
    }

    void TearDown() override {
        Kernel::fromHandle(kernelHandle)->destroy();
        ModuleFixture::tearDown();
    }

    ze_kernel_handle_t kernelHandle;
    L0::Kernel *kernel = nullptr;
};

TEST_F(KernelProgramBinaryTests, givenCallTozeKernelGetBinaryExpThenCorrectSizeAndDataReturned) {
    size_t kernelBinarySize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelGetBinaryExp(kernelHandle, &kernelBinarySize, nullptr));
    EXPECT_GT(kernelBinarySize, 0u);
    std::unique_ptr<uint8_t[]> kernelBinaryRetrieved = std::make_unique<uint8_t[]>(kernelBinarySize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelGetBinaryExp(kernelHandle, &kernelBinarySize, reinterpret_cast<uint8_t *>(kernelBinaryRetrieved.get())));

    auto &kernelImmutableData = this->module->kernelImmDatas.front();
    EXPECT_EQ(kernelBinarySize, kernelImmutableData->getKernelInfo()->heapInfo.kernelHeapSize);
    const char *heapPtr = reinterpret_cast<const char *>(kernelImmutableData->getKernelInfo()->heapInfo.pKernelHeap);
    EXPECT_EQ(0, memcmp(kernelBinaryRetrieved.get(), heapPtr, kernelBinarySize));
}

TEST_F(KernelProgramBinaryTests, givenCallToGetKernelProgramBinaryThenCorrectSizeAndDataReturned) {
    size_t kernelBinarySize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getKernelProgramBinary(&kernelBinarySize, nullptr));
    EXPECT_GT(kernelBinarySize, 0u);
    std::unique_ptr<char[]> kernelBinaryRetrieved = std::make_unique<char[]>(kernelBinarySize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getKernelProgramBinary(&kernelBinarySize, reinterpret_cast<char *>(kernelBinaryRetrieved.get())));

    auto &kernelImmutableData = this->module->kernelImmDatas.front();
    EXPECT_EQ(kernelBinarySize, kernelImmutableData->getKernelInfo()->heapInfo.kernelHeapSize);
    const char *heapPtr = reinterpret_cast<const char *>(kernelImmutableData->getKernelInfo()->heapInfo.pKernelHeap);
    EXPECT_EQ(0, memcmp(kernelBinaryRetrieved.get(), heapPtr, kernelBinarySize));
}

TEST_F(KernelProgramBinaryTests, givenCallToGetKernelProgramBinaryWithSmallSizeThenSmallSizeReturned) {
    size_t kernelBinarySize = 0;
    char *kernelBinaryRetrieved = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getKernelProgramBinary(&kernelBinarySize, kernelBinaryRetrieved));
    kernelBinaryRetrieved = new char[kernelBinarySize];
    size_t kernelBinarySize2 = kernelBinarySize / 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getKernelProgramBinary(&kernelBinarySize2, kernelBinaryRetrieved));
    EXPECT_EQ(kernelBinarySize2, (kernelBinarySize / 2));

    auto &kernelImmutableData = this->module->kernelImmDatas.front();
    EXPECT_EQ(kernelBinarySize, kernelImmutableData->getKernelInfo()->heapInfo.kernelHeapSize);
    const char *heapPtr = reinterpret_cast<const char *>(kernelImmutableData->getKernelInfo()->heapInfo.pKernelHeap);
    EXPECT_EQ(0, memcmp(kernelBinaryRetrieved, heapPtr, kernelBinarySize2));
    delete[] kernelBinaryRetrieved;
}

TEST_F(KernelProgramBinaryTests, givenCallToGetKernelProgramBinaryWithLargeSizeThenCorrectSizeReturned) {
    size_t kernelBinarySize = 0;
    char *kernelBinaryRetrieved = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getKernelProgramBinary(&kernelBinarySize, kernelBinaryRetrieved));
    kernelBinaryRetrieved = new char[kernelBinarySize];
    size_t kernelBinarySize2 = kernelBinarySize * 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getKernelProgramBinary(&kernelBinarySize2, kernelBinaryRetrieved));
    EXPECT_EQ(kernelBinarySize2, kernelBinarySize);

    auto &kernelImmutableData = this->module->kernelImmDatas.front();
    EXPECT_EQ(kernelBinarySize2, kernelImmutableData->getKernelInfo()->heapInfo.kernelHeapSize);
    const char *heapPtr = reinterpret_cast<const char *>(kernelImmutableData->getKernelInfo()->heapInfo.pKernelHeap);
    EXPECT_EQ(0, memcmp(kernelBinaryRetrieved, heapPtr, kernelBinarySize2));
    delete[] kernelBinaryRetrieved;
}

using KernelWorkDimTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelWorkDimTests, givenGroupCountsWhenPatchingWorkDimThenCrossThreadDataIsPatched) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    auto kernel = std::make_unique<MockKernel>(module.get());
    createKernel(kernel.get());
    kernel->setCrossThreadData(sizeof(uint32_t));

    mockKernelImmData->mockKernelDescriptor->payloadMappings.dispatchTraits.workDim = 0x0u;

    auto destinationBuffer = ArrayRef<const uint8_t>(kernel->getCrossThreadData(), kernel->getCrossThreadDataSize());
    auto &kernelDescriptor = mockKernelImmData->getDescriptor();
    auto workDimInCrossThreadDataPtr = destinationBuffer.begin() + kernelDescriptor.payloadMappings.dispatchTraits.workDim;
    EXPECT_EQ(*workDimInCrossThreadDataPtr, 0u);

    std::array<std::array<uint32_t, 7>, 8> sizesCountsWorkDim = {{{2, 1, 1, 1, 1, 1, 1},
                                                                  {1, 1, 1, 1, 1, 1, 1},
                                                                  {1, 2, 1, 2, 1, 1, 2},
                                                                  {1, 2, 1, 1, 1, 1, 2},
                                                                  {1, 1, 1, 1, 2, 1, 2},
                                                                  {1, 1, 1, 2, 2, 2, 3},
                                                                  {1, 1, 2, 1, 1, 1, 3},
                                                                  {1, 1, 1, 1, 1, 2, 3}}};

    for (auto &[groupSizeX, groupSizeY, groupSizeZ, groupCountX, groupCountY, groupCountZ, expectedWorkDim] : sizesCountsWorkDim) {
        ze_result_t res = kernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
        EXPECT_EQ(res, ZE_RESULT_SUCCESS);
        kernel->setGroupCount(groupCountX, groupCountY, groupCountZ);
        EXPECT_EQ(*workDimInCrossThreadDataPtr, expectedWorkDim);
    }
}

using KernelPrintHandlerTest = Test<ModuleFixture>;
struct MyPrintfHandler : public PrintfHandler {
    static uint32_t getPrintfSurfaceInitialDataSize() {
        return PrintfHandler::printfSurfaceInitialDataSize;
    }
};

TEST_F(KernelPrintHandlerTest, whenPrintPrintfOutputIsCalledThenPrintfBufferIsUsed) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    kernel = std::make_unique<WhiteBox<::L0::KernelImp>>();
    kernel->module = module.get();
    kernel->initialize(&desc);

    EXPECT_FALSE(kernel->printfBuffer == nullptr);
    kernel->printPrintfOutput(false);
    auto buffer = *reinterpret_cast<uint32_t *>(kernel->printfBuffer->getUnderlyingBuffer());
    EXPECT_EQ(buffer, MyPrintfHandler::getPrintfSurfaceInitialDataSize());
}

using PrintfKernelOwnershipTest = Test<ModuleFixture>;

TEST_F(PrintfKernelOwnershipTest, givenKernelWithPrintfThenModuleStoresItCorrectly) {
    ze_kernel_handle_t kernelHandle;
    ze_kernel_desc_t kernelDesc{};
    kernelDesc.pKernelName = kernelName.c_str();
    EXPECT_EQ(ZE_RESULT_SUCCESS, module->createKernel(&kernelDesc, &kernelHandle));

    auto kernel = static_cast<Mock<KernelImp> *>(Kernel::fromHandle(kernelHandle));
    EXPECT_NE(nullptr, kernel->getPrintfBufferAllocation());
    EXPECT_NE(nullptr, kernel->getDevicePrintfKernelMutex());
    EXPECT_EQ(1u, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
}

TEST_F(PrintfKernelOwnershipTest, givenKernelWithPrintfDestroyedFirstThenModuleDestructionReturnsTheCorrectStatus) {
    ze_kernel_handle_t kernelHandle;
    ze_kernel_desc_t kernelDesc{};
    kernelDesc.pKernelName = kernelName.c_str();
    EXPECT_EQ(ZE_RESULT_SUCCESS, module->createKernel(&kernelDesc, &kernelHandle));

    auto kernel = static_cast<Mock<KernelImp> *>(Kernel::fromHandle(kernelHandle));
    EXPECT_NE(nullptr, kernel->getPrintfBufferAllocation());
    EXPECT_NE(nullptr, kernel->getDevicePrintfKernelMutex());
    EXPECT_EQ(1u, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());

    EXPECT_EQ(1u, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().size());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, static_cast<ModuleImp *>(module.get())->destroyPrintfKernel(kernelHandle));
}

using PrintfTest = Test<DeviceFixture>;

TEST_F(PrintfTest, givenKernelWithPrintfThenPrintfBufferIsCreated) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel.module = &mockModule;

    EXPECT_TRUE(mockKernel.getImmutableData()->getDescriptor().kernelAttributes.flags.usesPrintf);

    mockKernel.createPrintfBuffer();
    EXPECT_NE(nullptr, mockKernel.getPrintfBufferAllocation());
}

TEST_F(PrintfTest, GivenKernelNotUsingPrintfWhenCreatingPrintfBufferThenAllocationIsNotCreated) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = false;
    mockKernel.module = &mockModule;

    mockKernel.createPrintfBuffer();
    EXPECT_EQ(nullptr, mockKernel.getPrintfBufferAllocation());
}

TEST_F(PrintfTest, WhenCreatingPrintfBufferThenAllocationAddedToResidencyContainer) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel.module = &mockModule;

    mockKernel.createPrintfBuffer();

    auto printfBufferAllocation = mockKernel.getPrintfBufferAllocation();
    EXPECT_NE(nullptr, printfBufferAllocation);

    EXPECT_NE(0u, mockKernel.internalResidencyContainer.size());
    EXPECT_EQ(mockKernel.internalResidencyContainer[mockKernel.internalResidencyContainer.size() - 1], printfBufferAllocation);
}

TEST_F(PrintfTest, WhenCreatingPrintfBufferThenCrossThreadDataIsPatched) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel.module = &mockModule;

    auto crossThreadData = std::make_unique<uint32_t[]>(4);

    mockKernel.descriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless = 0;
    mockKernel.descriptor.payloadMappings.implicitArgs.printfSurfaceAddress.pointerSize = sizeof(uintptr_t);
    mockKernel.crossThreadData.reset(reinterpret_cast<uint8_t *>(crossThreadData.get()));
    mockKernel.crossThreadDataSize = sizeof(uint32_t[4]);

    mockKernel.createPrintfBuffer();

    auto printfBufferAllocation = mockKernel.getPrintfBufferAllocation();
    EXPECT_NE(nullptr, printfBufferAllocation);

    auto printfBufferAddressPatched = *reinterpret_cast<uintptr_t *>(crossThreadData.get());
    auto printfBufferGpuAddressOffset = static_cast<uintptr_t>(printfBufferAllocation->getGpuAddressToPatch());
    EXPECT_EQ(printfBufferGpuAddressOffset, printfBufferAddressPatched);

    mockKernel.crossThreadData.release();
}

using PrintfHandlerTests = ::testing::Test;

HWTEST_F(PrintfHandlerTests, givenKernelWithPrintfWhenPrintingOutputWithBlitterUsedThenBlitterCopiesBuffer) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    {
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        auto kernelInfo = std::make_unique<KernelInfo>();
        kernelInfo->heapInfo.kernelHeapSize = 1;
        char kernelHeap[1];
        kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, nullptr, nullptr, false);

        auto &kernelDescriptor = kernelInfo->kernelDescriptor;
        kernelDescriptor.kernelAttributes.flags.usesPrintf = true;
        kernelDescriptor.kernelAttributes.flags.usesStringMapForPrintf = true;
        kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
        kernelDescriptor.kernelAttributes.gpuPointerSize = 8u;
        std::string expectedString("test123");
        kernelDescriptor.kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

        constexpr size_t size = 128;
        uint64_t gpuAddress = 0x2000;
        uint32_t bufferArray[size] = {};
        void *buffer = reinterpret_cast<void *>(bufferArray);
        NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
        auto printfAllocation = reinterpret_cast<uint32_t *>(buffer);
        printfAllocation[0] = 8;
        printfAllocation[1] = 0;

        StreamCapture capture;
        capture.captureStdout();
        PrintfHandler::printOutput(kernelImmutableData.get(), &mockAllocation, &deviceImp, true);
        std::string output = capture.getCapturedStdout();

        auto bcsEngine = device->tryGetEngine(NEO::EngineHelpers::getBcsEngineType(device->getRootDeviceEnvironment(), device->getDeviceBitfield(), device->getSelectorCopyEngine(), true), EngineUsage::internal);
        if (bcsEngine) {
            EXPECT_EQ(0u, output.size()); // memory is not actually copied with blitter in ULTs
            auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine->commandStreamReceiver);
            EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
            EXPECT_EQ(BlitterConstants::BlitDirection::bufferToHostPtr, bcsCsr->receivedBlitProperties[0].blitDirection);
            EXPECT_EQ(size, bcsCsr->receivedBlitProperties[0].copySize[0]);
        } else {
            EXPECT_STREQ(expectedString.c_str(), output.c_str());
        }
    }
}

HWTEST_F(PrintfHandlerTests, givenPrintDebugMessagesAndKernelWithPrintfWhenBlitterHangsThenErrorIsPrintedAndPrintfBufferPrinted) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    {
        auto bcsEngine = device->tryGetEngine(NEO::EngineHelpers::getBcsEngineType(device->getRootDeviceEnvironment(), device->getDeviceBitfield(), device->getSelectorCopyEngine(), true), EngineUsage::internal);
        if (!bcsEngine) {
            GTEST_SKIP();
        }
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine->commandStreamReceiver);
        bcsCsr->callBaseFlushBcsTask = false;
        bcsCsr->flushBcsTaskReturnValue = NEO::CompletionStamp::gpuHang;

        auto kernelInfo = std::make_unique<KernelInfo>();
        kernelInfo->heapInfo.kernelHeapSize = 1;
        char kernelHeap[1];
        kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, nullptr, nullptr, false);

        auto &kernelDescriptor = kernelInfo->kernelDescriptor;
        kernelDescriptor.kernelAttributes.flags.usesPrintf = true;
        kernelDescriptor.kernelAttributes.flags.usesStringMapForPrintf = true;
        kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
        kernelDescriptor.kernelAttributes.gpuPointerSize = 8u;
        std::string expectedString("test123");
        kernelDescriptor.kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

        constexpr size_t size = 128;
        uint64_t gpuAddress = 0x2000;
        uint32_t bufferArray[size] = {};
        void *buffer = reinterpret_cast<void *>(bufferArray);
        NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
        auto printfAllocation = reinterpret_cast<uint32_t *>(buffer);
        printfAllocation[0] = 8;
        printfAllocation[1] = 0;

        StreamCapture capture;
        capture.captureStdout();
        testing::internal::CaptureStderr();
        PrintfHandler::printOutput(kernelImmutableData.get(), &mockAllocation, &deviceImp, true);
        std::string output = capture.getCapturedStdout();
        std::string error = testing::internal::GetCapturedStderr();

        EXPECT_EQ(1u, bcsCsr->blitBufferCalled);
        EXPECT_EQ(BlitterConstants::BlitDirection::bufferToHostPtr, bcsCsr->receivedBlitProperties[0].blitDirection);
        EXPECT_EQ(size, bcsCsr->receivedBlitProperties[0].copySize[0]);

        EXPECT_STREQ(expectedString.c_str(), output.c_str());
        EXPECT_STREQ("Failed to copy printf buffer.\n", error.c_str());
    }
}

using KernelPatchtokensPrintfStringMapTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelPatchtokensPrintfStringMapTests, givenKernelWithPrintfStringsMapUsageEnabledWhenPrintOutputThenProperStringIsPrinted) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.usesPrintf = true;
    kernelDescriptor->kernelAttributes.flags.usesStringMapForPrintf = true;
    kernelDescriptor->kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
    std::string expectedString("test123");
    kernelDescriptor->kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto printfAllocation = reinterpret_cast<uint32_t *>(kernel->getPrintfBufferAllocation()->getUnderlyingBuffer());
    printfAllocation[0] = 8;
    printfAllocation[1] = 0;

    StreamCapture capture;
    capture.captureStdout();
    kernel->printPrintfOutput(false);
    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ(expectedString.c_str(), output.c_str());
}

TEST_F(KernelPatchtokensPrintfStringMapTests, givenKernelWithPrintfStringsMapUsageDisabledAndNoImplicitArgsWhenPrintOutputThenNothingIsPrinted) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.usesPrintf = true;
    kernelDescriptor->kernelAttributes.flags.usesStringMapForPrintf = false;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
    std::string expectedString("test123");
    kernelDescriptor->kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto printfAllocation = reinterpret_cast<uint32_t *>(kernel->getPrintfBufferAllocation()->getUnderlyingBuffer());
    printfAllocation[0] = 8;
    printfAllocation[1] = 0;

    StreamCapture capture;
    capture.captureStdout();
    kernel->printPrintfOutput(false);
    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ("", output.c_str());
}

TEST_F(KernelPatchtokensPrintfStringMapTests, givenKernelWithPrintfStringsMapUsageDisabledAndWithImplicitArgsWhenPrintOutputThenOutputIsPrinted) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.usesPrintf = true;
    kernelDescriptor->kernelAttributes.flags.usesStringMapForPrintf = false;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor->kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
    std::string expectedString("test123");
    kernelDescriptor->kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto printfAllocation = reinterpret_cast<uint32_t *>(kernel->getPrintfBufferAllocation()->getUnderlyingBuffer());
    printfAllocation[0] = 8;
    printfAllocation[1] = 0;

    StreamCapture capture;
    capture.captureStdout();
    kernel->printPrintfOutput(false);
    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ(expectedString.c_str(), output.c_str());
}

using KernelImplicitArgTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelImplicitArgTests, givenKernelWithImplicitArgsWhenInitializeThenPrintfSurfaceIsCreatedAndProperlyPatchedInImplicitArgs) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesPrintf = false;

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    auto pImplicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, pImplicitArgs);

    auto printfSurface = kernel->getPrintfBufferAllocation();
    ASSERT_NE(nullptr, printfSurface);

    EXPECT_NE(0u, pImplicitArgs->v0.printfBufferPtr);
    EXPECT_EQ(printfSurface->getGpuAddress(), pImplicitArgs->v0.printfBufferPtr);
}

TEST_F(KernelImplicitArgTests, givenImplicitArgsRequiredWhenCreatingKernelThenImplicitArgsAreCreated) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    auto pImplicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, pImplicitArgs);

    EXPECT_EQ(ImplicitArgsV0::getSize(), pImplicitArgs->v0.header.structSize);
    EXPECT_EQ(0u, pImplicitArgs->v0.header.structVersion);
}

TEST_F(KernelImplicitArgTests, givenKernelWithImplicitArgsWhenSettingKernelParamsThenImplicitArgsAreUpdated) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    auto simd = mockKernelImmData->kernelDescriptor->kernelAttributes.simdSize;

    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    auto pImplicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, pImplicitArgs);

    ImplicitArgsV0 expectedImplicitArgs{{ImplicitArgsV0::getSize(), 0}};

    expectedImplicitArgs.numWorkDim = 3;
    expectedImplicitArgs.simdWidth = simd;
    expectedImplicitArgs.localSizeX = 4;
    expectedImplicitArgs.localSizeY = 5;
    expectedImplicitArgs.localSizeZ = 6;
    expectedImplicitArgs.globalSizeX = 12;
    expectedImplicitArgs.globalSizeY = 10;
    expectedImplicitArgs.globalSizeZ = 6;
    expectedImplicitArgs.globalOffsetX = 1;
    expectedImplicitArgs.globalOffsetY = 2;
    expectedImplicitArgs.globalOffsetZ = 3;
    expectedImplicitArgs.groupCountX = 3;
    expectedImplicitArgs.groupCountY = 2;
    expectedImplicitArgs.groupCountZ = 1;
    expectedImplicitArgs.printfBufferPtr = kernel->getPrintfBufferAllocation()->getGpuAddress();

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();
    EXPECT_EQ(0, memcmp(pImplicitArgs, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

using BindlessKernelTest = Test<DeviceFixture>;

TEST_F(BindlessKernelTest, givenBindlessKernelWhenPatchingCrossThreadDataThenCorrectBindlessOffsetsAreWritten) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptorImg = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptorImg.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptorImg.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorImg.as<NEO::ArgDescImage>().bindless = sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptorImg);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 2 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor2);

    mockKernel.descriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 3 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 4 * sizeof(uint64_t);

    mockKernel.isBindlessOffsetSet.resize(4, 0);
    mockKernel.usingSurfaceStateHeap.resize(4, 0);

    mockKernel.descriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(5 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 5 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto patchValue1 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress));
    auto patchValue2 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + 1 * surfaceStateSize));
    auto patchValue3 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + 2 * surfaceStateSize));
    auto patchValue4 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + 3 * surfaceStateSize));

    mockKernel.patchBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(patchValue1, crossThreadData[0]);
    EXPECT_EQ(patchValue2, crossThreadData[1]);
    EXPECT_EQ(0u, crossThreadData[2]);

    if (neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper() == nullptr) {
        EXPECT_EQ(patchValue3, crossThreadData[3]);
        EXPECT_EQ(patchValue4, crossThreadData[4]);
    } else {
        EXPECT_EQ(0u, crossThreadData[3]);
        EXPECT_EQ(0u, crossThreadData[4]);
    }
}

TEST_F(BindlessKernelTest, givenBindlessKernelWithPatchedBindlessOffsetsWhenPatchingCrossThreadDataThenMemoryIsNotPatched) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptorImg = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptorImg.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptorImg.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorImg.as<NEO::ArgDescImage>().bindless = sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptorImg);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 2 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor2);

    mockKernel.isBindlessOffsetSet.resize(4, 1);
    mockKernel.isBindlessOffsetSet[1] = false;

    mockKernel.descriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(4 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 4 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto patchValue2 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + surfaceStateSize));

    mockKernel.patchBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(0u, crossThreadData[0]);
    EXPECT_EQ(patchValue2, crossThreadData[1]);
    EXPECT_EQ(0u, crossThreadData[2]);
    EXPECT_EQ(0u, crossThreadData[3]);
}

TEST_F(BindlessKernelTest, givenNoEntryInBindlessOffsetsMapWhenPatchingCrossThreadDataThenMemoryIsNotPatched) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(4 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 4 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    mockKernel.patchBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(0u, crossThreadData[0]);
}

TEST_F(BindlessKernelTest, givenNoStatefulArgsWhenPatchingBindlessOffsetsInCrossThreadDataThenMemoryIsNotPatched) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTValue);
    argDescriptor.as<NEO::ArgDescValue>() = NEO::ArgDescValue();
    argDescriptor.as<NEO::ArgDescValue>().elements.push_back(NEO::ArgDescValue::Element{0, 8, 0, false});
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(sizeof(uint64_t));
    mockKernel.crossThreadDataSize = sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    mockKernel.patchBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(0u, crossThreadData[0]);
}

TEST_F(BindlessKernelTest, givenGlobalBindlessAllocatorAndBindlessKernelWithImplicitArgsWhenPatchingCrossThreadDataThenMemoryIsNotPatched) {

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice,
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1);

    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    mockKernel.descriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 0x8;
    mockKernel.descriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless = NEO::undefined<NEO::SurfaceStateHeapOffset>;

    mockKernel.isBindlessOffsetSet.resize(1, 1);
    mockKernel.isBindlessOffsetSet[0] = true;

    mockKernel.descriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(4 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 4 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    mockKernel.patchBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(0u, crossThreadData[0]);
    EXPECT_EQ(0u, crossThreadData[1]);
    EXPECT_EQ(0u, crossThreadData[2]);
    EXPECT_EQ(0u, crossThreadData[3]);
}

TEST(KernelImmutableDataTest, givenBindlessKernelWhenInitializingImmDataThenSshTemplateIsAllocated) {
    HardwareInfo hwInfo = *defaultHwInfo;

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    {
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        auto kernelInfo = std::make_unique<KernelInfo>();
        kernelInfo->heapInfo.kernelHeapSize = 1;
        char kernelHeap[1];
        kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
        kernelInfo->heapInfo.surfaceStateHeapSize = 0;
        kernelInfo->heapInfo.pSsh = nullptr;

        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        kernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
        kernelInfo->kernelDescriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

        auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
        argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
        argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
        argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
        kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful = 1;

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, nullptr, nullptr, false);

        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());

        EXPECT_EQ(surfaceStateSize, kernelImmutableData->getSurfaceStateHeapSize());
    }
}

TEST_F(BindlessKernelTest, givenBindlessKernelWhenPatchingSamplerOffsetsInCrossThreadDataThenCorrectBindlessOffsetsAreWritten) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptorSampler = NEO::ArgDescriptor(NEO::ArgDescriptor::argTSampler);
    argDescriptorSampler.as<NEO::ArgDescSampler>() = NEO::ArgDescSampler();
    argDescriptorSampler.as<NEO::ArgDescSampler>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorSampler.as<NEO::ArgDescSampler>().bindless = sizeof(uint64_t);
    argDescriptorSampler.as<NEO::ArgDescSampler>().size = sizeof(uint64_t);
    argDescriptorSampler.as<NEO::ArgDescSampler>().index = 1;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptorSampler);

    auto argDescriptorSampler2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTSampler);
    argDescriptorSampler2.as<NEO::ArgDescSampler>() = NEO::ArgDescSampler();
    argDescriptorSampler2.as<NEO::ArgDescSampler>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorSampler2.as<NEO::ArgDescSampler>().bindless = 2 * sizeof(uint64_t);
    argDescriptorSampler2.as<NEO::ArgDescSampler>().size = sizeof(uint64_t);
    argDescriptorSampler2.as<NEO::ArgDescSampler>().index = undefined<uint8_t>;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptorSampler2);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 2 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor2);

    mockKernel.descriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 3 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 4 * sizeof(uint64_t);

    mockKernel.isBindlessOffsetSet.resize(2, 0);
    mockKernel.usingSurfaceStateHeap.resize(2, 0);

    mockKernel.descriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(5 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 5 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    auto samplerStateSize = gfxCoreHelper.getSamplerStateSize();

    auto patchValue1 = (static_cast<uint32_t>(baseAddress + 1 * samplerStateSize));
    auto patchValue2 = 0u;

    mockKernel.patchSamplerBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(patchValue1, crossThreadData[1]);
    EXPECT_EQ(0u, patchValue2);
    EXPECT_EQ(0u, crossThreadData[2]);
}

TEST_F(BindlessKernelTest, givenBindlessKernelWithInlineSamplersWhenPatchingSamplerOffsetsInCrossThreadDataThenCorrectBindlessOffsetsAreWritten) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 2 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor2);

    mockKernel.descriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 3 * sizeof(uint64_t);
    mockKernel.descriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 4 * sizeof(uint64_t);

    NEO::KernelDescriptor::InlineSampler inlineSampler = {};
    inlineSampler.samplerIndex = 0;
    inlineSampler.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::clampBorder;
    inlineSampler.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::linear;
    inlineSampler.isNormalized = true;
    inlineSampler.bindless = 5 * sizeof(uint64_t);
    inlineSampler.size = sizeof(uint64_t);
    mockKernel.descriptor.inlineSamplers.push_back(inlineSampler);

    inlineSampler.samplerIndex = 1;
    inlineSampler.bindless = 6 * sizeof(uint64_t);
    inlineSampler.size = sizeof(uint64_t);
    mockKernel.descriptor.inlineSamplers.push_back(inlineSampler);

    mockKernel.descriptor.payloadMappings.samplerTable.numSamplers = 2;
    mockKernel.isBindlessOffsetSet.resize(2, 0);
    mockKernel.usingSurfaceStateHeap.resize(2, 0);

    mockKernel.descriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = std::make_unique<uint8_t[]>(7 * sizeof(uint64_t));
    mockKernel.crossThreadDataSize = 7 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData.get(), 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    auto samplerStateSize = gfxCoreHelper.getSamplerStateSize();

    auto patchValue1 = (static_cast<uint32_t>(baseAddress + 0 * samplerStateSize));
    auto patchValue2 = (static_cast<uint32_t>(baseAddress + 1 * samplerStateSize));

    mockKernel.patchSamplerBindlessOffsetsInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData.get(), mockKernel.crossThreadDataSize);

    EXPECT_EQ(patchValue1, crossThreadData[5]);
    EXPECT_EQ(patchValue2, crossThreadData[6]);
}

using KernelSyncBufferTest = Test<ModuleFixture>;

TEST_F(KernelSyncBufferTest, GivenSyncBufferArgWhenPatchingSyncBufferThenPtrIsCorrectlyPatchedInCrossThreadData) {
    Mock<KernelImp> kernel;
    neoDevice->incRefInternal();

    Mock<Module> mockModule(device, nullptr);
    kernel.module = &mockModule;

    kernel.crossThreadData = std::make_unique<uint8_t[]>(64);
    kernel.crossThreadDataSize = 64;

    auto &syncBuffer = kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress;
    syncBuffer.stateless = 0x8;
    syncBuffer.pointerSize = 8;

    NEO::MockGraphicsAllocation alloc;
    alloc.setGpuPtr(0xffff800300060000);
    alloc.allocationOffset = 0x0;

    size_t bufferOffset = 0u;

    kernel.patchSyncBuffer(&alloc, bufferOffset);

    auto patchValue = *reinterpret_cast<uint64_t *>(ptrOffset(kernel.crossThreadData.get(), syncBuffer.stateless));
    auto expectedPatchValue = ptrOffset(alloc.getGpuAddressToPatch(), bufferOffset);
    EXPECT_EQ(expectedPatchValue, patchValue);

    neoDevice->decRefInternal();
}

} // namespace ult
} // namespace L0

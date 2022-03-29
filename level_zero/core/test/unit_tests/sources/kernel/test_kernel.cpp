/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/kernel_info_from_patchtokens.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"

#include "level_zero/core/source/debugger/debugger_l0.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/kernel/kernel_hw.h"
#include "level_zero/core/source/kernel/sampler_patch_values.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/source/sampler/sampler_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace NEO {
void populatePointerKernelArg(ArgDescPointer &dst,
                              CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful, CrossThreadDataOffset bindless,
                              KernelDescriptor::AddressingMode addressingMode);
}

namespace L0 {
namespace ult {

using KernelInitTest = Test<ModuleImmutableDataFixture>;

TEST_F(KernelInitTest, givenKernelToInitWhenItHasUnknownArgThenUnknowKernelArgHandlerAssigned) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    kernel->initialize(&desc);
    EXPECT_EQ(kernel->kernelArgHandlers[0], &KernelImp::setArgUnknown);
    EXPECT_EQ(mockKernelImmData->getDescriptor().payloadMappings.explicitArgs[0].type, NEO::ArgDescriptor::ArgTUnknown);
}

using KernelBaseAddressTests = Test<ModuleImmutableDataFixture>;
TEST_F(KernelBaseAddressTests, whenQueryingKernelBaseAddressThenCorrectAddressIsReturned) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
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
    EXPECT_EQ(baseAddress, kernel->getImmutableData()->getKernelInfo()->kernelAllocation->getGpuAddress());
}

TEST(KernelArgTest, givenKernelWhenSetArgUnknownCalledThenSuccessRteurned) {
    Mock<Kernel> mockKernel;
    EXPECT_EQ(mockKernel.setArgUnknown(0, 0, nullptr), ZE_RESULT_SUCCESS);
}

struct MockKernelWithCallTracking : Mock<::L0::Kernel> {
    using ::L0::KernelImp::kernelArgInfos;

    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation) override {
        ++setArgBufferWithAllocCalled;
        return KernelImp::setArgBufferWithAlloc(argIndex, argVal, allocation);
    }
    size_t setArgBufferWithAllocCalled = 0u;
};

using SetKernelArgCacheTest = Test<ModuleFixture>;

TEST_F(SetKernelArgCacheTest, givenValidBufferArgumentWhenSetMultipleTimesThenSetArgBufferWithAllocOnlyCalledIfNeeded) {
    MockKernelWithCallTracking mockKernel;
    mockKernel.module = module.get();
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernel.initialize(&desc);

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto allocationProperties = NEO::SVMAllocsManager::SvmAllocationProperties{};
    auto svmAllocation = svmAllocsManager->createSVMAlloc(4096, allocationProperties, context->rootDeviceIndices, context->deviceBitfields);

    size_t callCounter = 0u;

    //first setArg - called
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    //same setArg but allocationCounter == 0 - called
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    //same setArg - not called and argInfo.allocationCounter is updated
    ++svmAllocsManager->allocationsCounter;
    EXPECT_EQ(0u, mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);
    EXPECT_EQ(svmAllocsManager->allocationsCounter, mockKernel.kernelArgInfos[0].allocIdMemoryManagerCounter);

    //same setArg and allocationCounter - not called
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);

    //same setArg but different allocId - called
    svmAllocsManager->getSVMAlloc(svmAllocation)->setAllocId(1u);
    ++svmAllocsManager->allocationsCounter;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(svmAllocation), &svmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    //different value - called
    auto secondSvmAllocation = svmAllocsManager->createSVMAlloc(4096, allocationProperties, context->rootDeviceIndices, context->deviceBitfields);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mockKernel.setArgBuffer(0, sizeof(secondSvmAllocation), &secondSvmAllocation));
    EXPECT_EQ(++callCounter, mockKernel.setArgBufferWithAllocCalled);

    //same value but no svmData - ZE_RESULT_ERROR_INVALID_ARGUMENT
    svmAllocsManager->freeSVMAlloc(secondSvmAllocation);
    ++svmAllocsManager->allocationsCounter;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, mockKernel.setArgBuffer(0, sizeof(secondSvmAllocation), &secondSvmAllocation));
    EXPECT_EQ(callCounter, mockKernel.setArgBufferWithAllocCalled);

    svmAllocsManager->freeSVMAlloc(svmAllocation);
}

using KernelImpSetGroupSizeTest = Test<DeviceFixture>;

TEST_F(KernelImpSetGroupSizeTest, WhenCalculatingLocalIdsThenGrfSizeIsTakenFromCapabilityTable) {
    Mock<Kernel> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    mockKernel.descriptor.kernelAttributes.simdSize = 1;
    mockKernel.descriptor.kernelAttributes.numLocalIdChannels = 3;
    mockKernel.module = &mockModule;
    auto grfSize = mockModule.getDevice()->getHwInfo().capabilityTable.grfSize;
    uint32_t groupSize[3] = {2, 3, 5};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(groupSize[0] * groupSize[1] * groupSize[2], mockKernel.numThreadsPerThreadGroup);
    EXPECT_EQ(grfSize * groupSize[0] * groupSize[1] * groupSize[2], mockKernel.perThreadDataSizeForWholeThreadGroup);
    ASSERT_LE(grfSize * groupSize[0] * groupSize[1] * groupSize[2], mockKernel.perThreadDataSizeForWholeThreadGroup);
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
    Mock<Kernel> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    mockKernel.descriptor.kernelAttributes.simdSize = 1;
    mockKernel.module = &mockModule;
    mockKernel.kernelRequiresGenerationOfLocalIdsByRuntime = false;

    uint32_t groupSize[3] = {2, 3, 5};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(groupSize[0] * groupSize[1] * groupSize[2], mockKernel.numThreadsPerThreadGroup);
    EXPECT_EQ(0u, mockKernel.perThreadDataSizeForWholeThreadGroup);
    EXPECT_EQ(0u, mockKernel.perThreadDataSize);
    EXPECT_EQ(nullptr, mockKernel.perThreadDataForWholeThreadGroup);
}

TEST_F(KernelImpSetGroupSizeTest, givenIncorrectGroupSizeWhenSettingGroupSizeThenInvalidGroupSizeDimensionErrorIsReturned) {
    Mock<Kernel> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    for (auto i = 0u; i < 3u; i++) {
        mockKernel.descriptor.kernelAttributes.requiredWorkgroupSize[i] = 2;
    }
    mockKernel.module = &mockModule;

    uint32_t groupSize[3] = {1, 1, 1};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, ret);
}

TEST_F(KernelImpSetGroupSizeTest, givenZeroGroupSizeWhenSettingGroupSizeThenInvalidArgumentErrorIsReturned) {
    Mock<Kernel> mockKernel;
    Mock<Module> mockModule(this->device, nullptr);
    for (auto i = 0u; i < 3u; i++) {
        mockKernel.descriptor.kernelAttributes.requiredWorkgroupSize[i] = 2;
    }
    mockKernel.module = &mockModule;

    uint32_t groupSize[3] = {0, 0, 0};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);
}

using SetKernelArg = Test<ModuleFixture>;
using ImageSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

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

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto handle = imageHW->toHandle();
    auto imgInfo = imageHW->getImageInfo();
    auto pixelSize = imgInfo.surfaceFormat->ImageElementSizeInBytes;

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

    auto sampler = std::make_unique<WhiteBox<::L0::SamplerCoreFamily<gfxCoreFamily>>>();

    auto ret = sampler->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto handle = sampler->toHandle();

    kernel->setArgSampler(5, sizeof(sampler.get()), &handle);

    auto crossThreadData = kernel->getCrossThreadData();

    auto pSamplerSnapWa = ptrOffset(crossThreadData, samplerArg.metadataPayload.samplerSnapWa);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), *reinterpret_cast<const uint32_t *>(pSamplerSnapWa));

    auto pSamplerAddressingMode = ptrOffset(crossThreadData, samplerArg.metadataPayload.samplerAddressingMode);
    EXPECT_EQ(static_cast<uint32_t>(SamplerPatchValues::AddressClampToBorder), *pSamplerAddressingMode);

    auto pSamplerNormalizedCoords = ptrOffset(crossThreadData, samplerArg.metadataPayload.samplerNormalizedCoords);
    EXPECT_EQ(static_cast<uint32_t>(SamplerPatchValues::NormalizedCoordsTrue), *pSamplerNormalizedCoords);
}

using ArgSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(SetKernelArg, givenBufferArgumentWhichHasNotBeenAllocatedByRuntimeThenInvalidArgumentIsReturned, ArgSupport) {
    createKernel();

    uint64_t hostAddress = 0x1234;

    ze_result_t res = kernel->setArgBuffer(0, sizeof(hostAddress), &hostAddress);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

class KernelImmutableDataFixture : public ModuleImmutableDataFixture {
  public:
    void SetUp() {
        ModuleImmutableDataFixture::SetUp();
    }

    void TearDown() {
        ModuleImmutableDataFixture::TearDown();
    }
};

using KernelImmutableDataTests = Test<KernelImmutableDataFixture>;

TEST_F(KernelImmutableDataTests, givenKernelInitializedWithNoPrivateMemoryThenPrivateMemoryIsNull) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 0u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());

    EXPECT_EQ(nullptr, kernel->privateMemoryGraphicsAllocation);
}

TEST_F(KernelImmutableDataTests, givenKernelInitializedWithPrivateMemoryThenPrivateMemoryIsCreated) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());

    EXPECT_NE(nullptr, kernel->privateMemoryGraphicsAllocation);

    size_t expectedSize = perHwThreadPrivateMemorySizeRequested *
                          device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch;
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

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    size_t copyForGlobalSurface = 1u;
    auto copyForIsa = module->getKernelImmutableDataVector().size();
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
    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    uint32_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    mockKernelImmData->initialize(mockKernelImmData->mockKernelInfo, device,
                                  device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  module.get()->translationUnit->globalConstBuffer,
                                  module.get()->translationUnit->globalVarBuffer,
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
    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    uint32_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    mockKernelImmData->initialize(mockKernelImmData->mockKernelInfo, device,
                                  device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  module.get()->translationUnit->globalConstBuffer,
                                  module.get()->translationUnit->globalVarBuffer,
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
    mockKernelImmData->getIsaGraphicsAllocation()->setAllocationType(AllocationType::KERNEL_ISA_INTERNAL);

    size_t previouscopyMemoryToAllocationCalledTimes =
        mockMemoryManager->copyMemoryToAllocationCalledTimes;

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    size_t copyForGlobalSurface = 1u;
    size_t copyForPatchingIsa = 0u;
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

TEST_F(KernelImmutableDataTests, givenInternalModuleWhenKernelIsCreatedIsaIsNotCopiedDuringLinking) {
    NEO::MockCompilerEnableGuard mock(true);
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

    std::unique_ptr<L0::ult::MockModule> moduleMock = std::make_unique<L0::ult::MockModule>(device, moduleBuildLog, ModuleType::Builtin);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::Kernel> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelMock.immutableData.surfaceStateHeapSize = 64;
    kernelMock.immutableData.surfaceStateHeapTemplate.reset(new uint8_t[64]);
    kernelMock.immutableData.getIsaGraphicsAllocation()->setAllocationType(AllocationType::KERNEL_ISA_INTERNAL);
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    moduleMock->kernelImmData = &kernelMock.immutableData;

    size_t previouscopyMemoryToAllocationCalledTimes = mockMemoryManager->copyMemoryToAllocationCalledTimes;
    auto result = moduleMock->initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(result);
    size_t expectedPreviouscopyMemoryToAllocationCalledTimes = previouscopyMemoryToAllocationCalledTimes;

    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes, mockMemoryManager->copyMemoryToAllocationCalledTimes);

    for (auto &ki : moduleMock->kernelImmDatas) {
        EXPECT_FALSE(ki->isIsaCopiedToAllocation());
    }

    expectedPreviouscopyMemoryToAllocationCalledTimes++;

    ze_kernel_desc_t desc = {};
    desc.pKernelName = "";

    moduleMock->kernelImmData = moduleMock->kernelImmDatas[0].get();

    kernelMock.initialize(&desc);

    EXPECT_EQ(expectedPreviouscopyMemoryToAllocationCalledTimes, mockMemoryManager->copyMemoryToAllocationCalledTimes);
}

TEST_F(KernelImmutableDataTests, givenKernelInitializedWithPrivateMemoryThenContainerHasOneExtraSpaceForAllocation) {
    std::string testFile;
    retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(
        testFile.c_str(),
        size);
    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;
    ModuleBuildLog *moduleBuildLog = nullptr;

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    std::unique_ptr<MockModule> moduleWithPrivateMemory = std::make_unique<MockModule>(device,
                                                                                       moduleBuildLog,
                                                                                       ModuleType::User,
                                                                                       perHwThreadPrivateMemorySizeRequested,
                                                                                       mockKernelImmData.get());
    bool result = moduleWithPrivateMemory->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_TRUE(result);

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernelWithPrivateMemory;
    kernelWithPrivateMemory = std::make_unique<ModuleImmutableDataFixture::MockKernel>(moduleWithPrivateMemory.get());

    createKernel(kernelWithPrivateMemory.get());
    EXPECT_NE(nullptr, kernelWithPrivateMemory->privateMemoryGraphicsAllocation);

    size_t sizeContainerWithPrivateMemory = kernelWithPrivateMemory->getResidencyContainer().size();

    perHwThreadPrivateMemorySizeRequested = 0u;
    std::unique_ptr<MockImmutableData> mockKernelImmDataForModuleWithoutPrivateMemory = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    std::unique_ptr<MockModule> moduleWithoutPrivateMemory = std::make_unique<MockModule>(device,
                                                                                          moduleBuildLog,
                                                                                          ModuleType::User,
                                                                                          perHwThreadPrivateMemorySizeRequested,
                                                                                          mockKernelImmDataForModuleWithoutPrivateMemory.get());
    result = moduleWithoutPrivateMemory->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_TRUE(result);

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernelWithoutPrivateMemory;
    kernelWithoutPrivateMemory = std::make_unique<ModuleImmutableDataFixture::MockKernel>(moduleWithoutPrivateMemory.get());

    createKernel(kernelWithoutPrivateMemory.get());
    EXPECT_EQ(nullptr, kernelWithoutPrivateMemory->privateMemoryGraphicsAllocation);

    size_t sizeContainerWithoutPrivateMemory = kernelWithoutPrivateMemory->getResidencyContainer().size();

    EXPECT_EQ(sizeContainerWithoutPrivateMemory + 1u, sizeContainerWithPrivateMemory);
}

TEST_F(KernelImmutableDataTests, givenKernelWithPrivateMemoryBiggerThanGlobalMemoryThenPrivateMemoryIsNotAllocated) {
    std::string testFile;
    retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(
        testFile.c_str(),
        size);
    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;
    ModuleBuildLog *moduleBuildLog = nullptr;

    uint32_t perHwThreadPrivateMemorySizeRequested = std::numeric_limits<uint32_t>::max();
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
    std::unique_ptr<MockModule> module = std::make_unique<MockModule>(device,
                                                                      moduleBuildLog,
                                                                      ModuleType::User,
                                                                      perHwThreadPrivateMemorySizeRequested,
                                                                      mockKernelImmData.get());
    bool result = module->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_TRUE(result);
    EXPECT_TRUE(module->shouldAllocatePrivateMemoryPerDispatch());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    createKernel(kernel.get());
    EXPECT_EQ(nullptr, kernel->getPrivateMemoryGraphicsAllocation());
}

class KernelDescriptorRTCallsTrue : public NEO::KernelDescriptor {
    bool hasRTCalls() const override {
        return true;
    }
};

class KernelDescriptorRTCallsFalse : public NEO::KernelDescriptor {
    bool hasRTCalls() const override {
        return false;
    }
};

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueThenRayTracingIsInitialized) {
    KernelDescriptorRTCallsTrue mockDescriptor = {};
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
                                          ModuleType::User,
                                          32u,
                                          mockKernelImmutableData.get());
    module->maxGroupSize = 10;

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module.get()->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    neoDevice->setRTDispatchGlobalsForceAllocation();

    auto result = kernel->initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, module.get()->getDevice()->getNEODevice()->getRTMemoryBackedBuffer());

    auto rtDispatchGlobals = neoDevice->getRTDispatchGlobals(NEO::RayTracingHelper::maxBvhLevels);
    EXPECT_NE(nullptr, rtDispatchGlobals);

    size_t residencySize = kernel->getResidencyContainer().size();
    EXPECT_NE(0u, residencySize);

    EXPECT_EQ(kernel->getResidencyContainer()[residencySize - 1], rtDispatchGlobals);
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueButKernelDoesNotHaveRTDGAllocationTokenThenRayTracingStillEnabledWithoutAllocation) {
    KernelDescriptorRTCallsTrue mockDescriptor = {};
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
                                          ModuleType::User,
                                          32u,
                                          mockKernelImmutableData.get());
    module->maxGroupSize = 10;

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module.get()->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    auto result = kernel->initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, module.get()->getDevice()->getNEODevice()->getRTMemoryBackedBuffer());

    auto rtDispatchGlobals = neoDevice->getRTDispatchGlobals(NEO::RayTracingHelper::maxBvhLevels);
    EXPECT_EQ(nullptr, rtDispatchGlobals);
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueAndNoRTDispatchGlobalsIsAllocatedThenRayTracingIsNotInitialized) {
    KernelDescriptorRTCallsTrue mockDescriptor = {};
    mockDescriptor.kernelMetadata.kernelName = "rt_test";
    for (auto i = 0u; i < 3u; i++) {
        mockDescriptor.kernelAttributes.requiredWorkgroupSize[i] = 0;
    }
    mockDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize = 4;

    NEO::MemoryManager *currMemoryManager = new NEO::FailMemoryManager(0, *neoDevice->executionEnvironment);

    std::unique_ptr<MockImmutableData> mockKernelImmutableData =
        std::make_unique<MockImmutableData>(32u);
    mockKernelImmutableData->kernelDescriptor = &mockDescriptor;

    ModuleBuildLog *moduleBuildLog = nullptr;
    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::User,
                                          32u,
                                          mockKernelImmutableData.get());
    module->maxGroupSize = 10;

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";
    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module.get()->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(nullptr);
    neoDevice->injectMemoryManager(currMemoryManager);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, kernel->initialize(&kernelDesc));
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsFalseThenRayTracingIsNotInitialized) {
    KernelDescriptorRTCallsFalse mockDescriptor = {};
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
                                          ModuleType::User,
                                          32u,
                                          mockKernelImmutableData.get());
    module->maxGroupSize = 10;

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module.get()->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->initialize(&kernelDesc));
    EXPECT_EQ(nullptr, module.get()->getDevice()->getNEODevice()->getRTMemoryBackedBuffer());
}

TEST_F(KernelImmutableDataTests, whenHasRTCallsIsTrueThenCrossThreadDataIsPatched) {
    KernelDescriptorRTCallsTrue mockDescriptor = {};
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
                                          ModuleType::User,
                                          32u,
                                          mockKernelImmutableData.get());
    module->maxGroupSize = 10;

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "rt_test";

    auto immDataVector =
        const_cast<std::vector<std::unique_ptr<KernelImmutableData>> *>(&module.get()->getKernelImmutableDataVector());

    immDataVector->push_back(std::move(mockKernelImmutableData));

    auto crossThreadData = std::make_unique<uint32_t[]>(4);
    kernel->crossThreadData.reset(reinterpret_cast<uint8_t *>(crossThreadData.get()));
    kernel->crossThreadDataSize = sizeof(uint32_t[4]);

    neoDevice->setRTDispatchGlobalsForceAllocation();

    auto result = kernel->initialize(&kernelDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto rtDispatchGlobals = neoDevice->getRTDispatchGlobals(NEO::RayTracingHelper::maxBvhLevels);
    EXPECT_NE(nullptr, rtDispatchGlobals);

    auto dispatchGlobalsAddressPatched = *reinterpret_cast<uintptr_t *>(crossThreadData.get());
    auto dispatchGlobalsGpuAddressOffset = static_cast<uintptr_t>(rtDispatchGlobals->getGpuAddressToPatch());
    EXPECT_EQ(dispatchGlobalsGpuAddressOffset, dispatchGlobalsAddressPatched);

    kernel->crossThreadData.release();
}

using KernelIndirectPropertiesFromIGCTests = KernelImmutableDataTests;

TEST_F(KernelIndirectPropertiesFromIGCTests, whenInitializingKernelWithNoKernelLoadAndNoStoreAndNoAtomicThenHasIndirectAccessIsSetToFalse) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DisableIndirectAccess.set(0);

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
    module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;

    kernel->initialize(&desc);

    EXPECT_FALSE(kernel->hasIndirectAccess());
}

TEST_F(KernelIndirectPropertiesFromIGCTests, whenInitializingKernelWithKernelLoadStoreAtomicThenHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DisableIndirectAccess.set(0);

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get());

    {
        std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
        kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());

        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgLoad = true;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgStore = false;
        module->mockKernelImmData->mockKernelDescriptor->kernelAttributes.hasNonKernelArgAtomic = false;

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

        kernel->initialize(&desc);

        EXPECT_TRUE(kernel->hasIndirectAccess());
    }
}

class KernelPropertiesTests : public ModuleFixture, public ::testing::Test {
  public:
    class MockKernel : public KernelImp {
      public:
        using KernelImp::kernelHasIndirectAccess;
    };
    void SetUp() override {
        ModuleFixture::SetUp();

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();

        ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        kernel = static_cast<MockKernel *>(L0::Kernel::fromHandle(kernelHandle));
        kernel->kernelHasIndirectAccess = true;
    }

    void TearDown() override {
        Kernel::fromHandle(kernelHandle)->destroy();
        ModuleFixture::TearDown();
    }

    ze_kernel_handle_t kernelHandle;
    MockKernel *kernel = nullptr;
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

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(6U, kernelProperties.numKernelArgs);

    EXPECT_EQ(0U, kernelProperties.requiredNumSubGroups);
    EXPECT_EQ(0U, kernelProperties.requiredSubgroupSize);

    uint32_t maxSubgroupSize = this->kernel->getKernelDescriptor().kernelAttributes.simdSize;
    ASSERT_NE(0U, maxSubgroupSize);
    EXPECT_EQ(maxSubgroupSize, kernelProperties.maxSubgroupSize);

    uint32_t maxKernelWorkGroupSize = static_cast<uint32_t>(this->module->getDevice()->getNEODevice()->getDeviceInfo().maxWorkGroupSize);
    uint32_t maxNumSubgroups = maxKernelWorkGroupSize / maxSubgroupSize;
    EXPECT_EQ(maxNumSubgroups, kernelProperties.maxNumSubgroups);

    EXPECT_EQ(sizeof(float) * 16U, kernelProperties.localMemSize);
    EXPECT_EQ(0U, kernelProperties.privateMemSize);
    EXPECT_EQ(0U, kernelProperties.spillMemSize);

    uint8_t zeroKid[ZE_MAX_KERNEL_UUID_SIZE];
    uint8_t zeroMid[ZE_MAX_MODULE_UUID_SIZE];
    memset(&zeroKid, 0, ZE_MAX_KERNEL_UUID_SIZE);
    memset(&zeroMid, 0, ZE_MAX_MODULE_UUID_SIZE);
    EXPECT_EQ(0, memcmp(&kernelProperties.uuid.kid, &zeroKid,
                        sizeof(kernelProperties.uuid.kid)));
    EXPECT_EQ(0, memcmp(&kernelProperties.uuid.mid, &zeroMid,
                        sizeof(kernelProperties.uuid.mid)));
}

TEST_F(KernelPropertiesTests, whenPassingPreferredGroupSizeStructToGetPropertiesThenPreferredMultipleIsReturned) {
    ze_kernel_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_kernel_preferred_group_size_properties_t preferredGroupProperties = {};
    preferredGroupProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PREFERRED_GROUP_SIZE_PROPERTIES;

    kernelProperties.pNext = &preferredGroupProperties;

    ze_result_t res = kernel->getProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &hwHelper = NEO::HwHelper::get(module->getDevice()->getHwInfo().platform.eRenderCoreFamily);
    if (hwHelper.isFusedEuDispatchEnabled(module->getDevice()->getHwInfo(), false)) {
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
    NEO::DebugManager.flags.DisableIndirectAccess.set(0);

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

HWTEST2_F(KernelPropertiesTests, whenHasRTCallsIsTrueThenUsesRayTracingIsTrue, MatchAny) {
    WhiteBoxKernelHw<gfxCoreFamily> mockKernel;
    KernelDescriptorRTCallsTrue mockDescriptor = {};
    WhiteBox<::L0::KernelImmutableData> mockKernelImmutableData = {};

    mockKernelImmutableData.kernelDescriptor = &mockDescriptor;
    mockKernel.kernelImmData = &mockKernelImmutableData;

    EXPECT_TRUE(mockKernel.usesRayTracing());
}

HWTEST2_F(KernelPropertiesTests, whenHasRTCallsIsFalseThenUsesRayTracingIsFalse, MatchAny) {
    WhiteBoxKernelHw<gfxCoreFamily> mockKernel;
    KernelDescriptorRTCallsFalse mockDescriptor = {};
    WhiteBox<::L0::KernelImmutableData> mockKernelImmutableData = {};

    mockKernelImmutableData.kernelDescriptor = &mockDescriptor;
    mockKernel.kernelImmData = &mockKernelImmutableData;

    EXPECT_FALSE(mockKernel.usesRayTracing());
}

using KernelIndirectPropertiesTests = KernelPropertiesTests;

TEST_F(KernelIndirectPropertiesTests, whenCallingSetIndirectAccessWithKernelThatHasIndirectAccessThenIndirectAccessIsSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DisableIndirectAccess.set(0);
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
    NEO::DebugManager.flags.DisableIndirectAccess.set(1);
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
    NEO::DebugManager.flags.DisableIndirectAccess.set(0);
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

TEST_F(KernelIndirectPropertiesTests, whenCallingSetIndirectAccessWithKernelThatDoesNotHaveIndirectAccessThenIndirectAccessIsNotSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DisableIndirectAccess.set(0);
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
    EXPECT_FALSE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_FALSE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(unifiedMemoryControls.indirectSharedAllocationsAllowed);
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
    EXPECT_EQ(NEO::SlmPolicy::SlmPolicyNone, kernel->getSlmPolicy());
    ze_result_t res = kernel->setCacheConfig(ZE_CACHE_CONFIG_FLAG_LARGE_SLM);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(NEO::SlmPolicy::SlmPolicyLargeSlm, kernel->getSlmPolicy());
}

TEST_F(KernelPropertiesTests, givenValidKernelAndLargeDataIsSetThenForceLargeDataIsTrue) {
    EXPECT_EQ(NEO::SlmPolicy::SlmPolicyNone, kernel->getSlmPolicy());
    ze_result_t res = kernel->setCacheConfig(ZE_CACHE_CONFIG_FLAG_LARGE_DATA);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(NEO::SlmPolicy::SlmPolicyLargeData, kernel->getSlmPolicy());
}

using KernelLocalIdsTest = Test<ModuleFixture>;

TEST_F(KernelLocalIdsTest, WhenKernelIsCreatedThenDefaultLocalIdGenerationbyRuntimeIsTrue) {
    createKernel();

    EXPECT_TRUE(kernel->requiresGenerationOfLocalIdsByRuntime());
}

struct KernelIsaTests : Test<ModuleFixture> {
    void SetUp() override {
        Test<ModuleFixture>::SetUp();

        auto &capabilityTable = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable;
        bool createBcsEngine = !capabilityTable.blitterOperationsSupported;
        capabilityTable.blitterOperationsSupported = true;

        if (createBcsEngine) {
            auto &engine = device->getNEODevice()->getEngine(0);
            bcsOsContext.reset(OsContext::create(nullptr, 0,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, device->getNEODevice()->getDeviceBitfield())));
            engine.osContext = bcsOsContext.get();
            engine.commandStreamReceiver->setupContext(*bcsOsContext);
        }
    }

    std::unique_ptr<OsContext> bcsOsContext;
};

TEST_F(KernelIsaTests, givenKernelAllocationInLocalMemoryWhenCreatingWithoutAllowedCpuAccessThenUseBcsForTransfer) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(NEO::AllocationType::KERNEL_ISA) - 1));

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    auto bcsCsr = device->getNEODevice()->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver;
    auto initialTaskCount = bcsCsr->peekTaskCount();

    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);

    if (kernelImmutableData.getIsaGraphicsAllocation()->isAllocatedInLocalMemoryPool()) {
        EXPECT_EQ(initialTaskCount + 1, bcsCsr->peekTaskCount());
    } else {
        EXPECT_EQ(initialTaskCount, bcsCsr->peekTaskCount());
    }

    device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

TEST_F(KernelIsaTests, givenKernelAllocationInLocalMemoryWhenCreatingWithAllowedCpuAccessThenDontUseBcsForTransfer) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(NEO::AllocationType::KERNEL_ISA) - 1));

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    auto bcsCsr = device->getNEODevice()->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver;
    auto initialTaskCount = bcsCsr->peekTaskCount();

    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);

    EXPECT_EQ(initialTaskCount, bcsCsr->peekTaskCount());

    device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

TEST_F(KernelIsaTests, givenKernelAllocationInLocalMemoryWhenCreatingWithDisallowedCpuAccessAndDisabledBlitterThenFallbackToCpuCopy) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(NEO::AllocationType::KERNEL_ISA) - 1));

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    auto bcsCsr = device->getNEODevice()->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver;
    auto initialTaskCount = bcsCsr->peekTaskCount();

    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);

    EXPECT_EQ(initialTaskCount, bcsCsr->peekTaskCount());

    device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

TEST_F(KernelIsaTests, givenKernelInfoWhenInitializingImmutableDataWithInternalIsaThenCorrectAllocationTypeIsUsed) {
    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, true);
    EXPECT_EQ(NEO::AllocationType::KERNEL_ISA_INTERNAL, kernelImmutableData.getIsaGraphicsAllocation()->getAllocationType());
}

TEST_F(KernelIsaTests, givenKernelInfoWhenInitializingImmutableDataWithNonInternalIsaThenCorrectAllocationTypeIsUsed) {
    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);
    EXPECT_EQ(NEO::AllocationType::KERNEL_ISA, kernelImmutableData.getIsaGraphicsAllocation()->getAllocationType());
}

TEST_F(KernelIsaTests, givenKernelInfoWhenInitializingImmutableDataWithIsaThenPaddingIsAdded) {
    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);
    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);
    auto graphicsAllocation = kernelImmutableData.getIsaGraphicsAllocation();
    auto &hwHelper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    size_t isaPadding = hwHelper.getPaddingForISAAllocation();
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), kernelInfo.heapInfo.KernelHeapSize + isaPadding);
}

TEST_F(KernelIsaTests, givenGlobalBuffersWhenCreatingKernelImmutableDataThenBuffersAreAddedToResidencyContainer) {
    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1100;
    NEO::MockGraphicsAllocation globalVarBuffer(buffer, gpuAddress, size);
    NEO::MockGraphicsAllocation globalConstBuffer(buffer, gpuAddress, size);

    kernelImmutableData.initialize(&kernelInfo, device, 0,
                                   &globalConstBuffer, &globalVarBuffer, false);
    auto &resCont = kernelImmutableData.getResidencyContainer();
    EXPECT_EQ(1, std::count(resCont.begin(), resCont.end(), &globalVarBuffer));
    EXPECT_EQ(1, std::count(resCont.begin(), resCont.end(), &globalConstBuffer));
}

using KernelImpPatchBindlessTest = Test<ModuleFixture>;

TEST_F(KernelImpPatchBindlessTest, GivenKernelImpWhenPatchBindlessOffsetCalledThenOffsetPatchedCorrectly) {
    Mock<Kernel> kernel;
    neoDevice->incRefInternal();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());
    Mock<Module> mockModule(device, nullptr);
    kernel.module = &mockModule;
    NEO::MockGraphicsAllocation alloc;
    uint32_t bindless = 0x40;
    auto &hwHelper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    size_t size = hwHelper.getRenderSurfaceStateSize();
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &alloc, NEO::BindlessHeapsHelper::GLOBAL_SSH);
    auto patchLocation = ptrOffset(kernel.getCrossThreadData(), bindless);
    auto patchValue = hwHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(expectedSsInHeap.surfaceStateOffset));

    auto ssPtr = kernel.patchBindlessSurfaceState(&alloc, bindless);

    EXPECT_EQ(ssPtr, expectedSsInHeap.ssPtr);
    EXPECT_TRUE(memcmp(const_cast<uint8_t *>(patchLocation), &patchValue, sizeof(patchValue)) == 0);
    EXPECT_TRUE(std::find(kernel.getResidencyContainer().begin(), kernel.getResidencyContainer().end(), expectedSsInHeap.heapAllocation) != kernel.getResidencyContainer().end());
    neoDevice->decRefInternal();
}

HWTEST2_F(KernelImpPatchBindlessTest, GivenKernelImpWhenSetSurfaceStateBindlessThenSurfaceStateUpdated, MatchAny) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);
    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = 0x40;
    arg.bindful = undefined<SurfaceStateHeapOffset>;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());

    auto &hwHelper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    size_t size = hwHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::GLOBAL_SSH);

    memset(expectedSsInHeap.ssPtr, 0, size);
    auto surfaceStateBefore = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);
    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);

    auto surfaceStateAfter = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);

    EXPECT_FALSE(memcmp(&surfaceStateAfter, &surfaceStateBefore, size) == 0);
}

HWTEST2_F(KernelImpPatchBindlessTest, GivenKernelImpWhenSetSurfaceStateBindfulThenSurfaceStateNotUpdated, MatchAny) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = 0x40;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());

    auto &hwHelper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    size_t size = hwHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::GLOBAL_SSH);

    memset(expectedSsInHeap.ssPtr, 0, size);
    auto surfaceStateBefore = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);
    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);

    auto surfaceStateAfter = *reinterpret_cast<RENDER_SURFACE_STATE *>(expectedSsInHeap.ssPtr);

    EXPECT_TRUE(memcmp(&surfaceStateAfter, &surfaceStateBefore, size) == 0);
}

using KernelImpL3CachingTests = Test<ModuleFixture>;

HWTEST2_F(KernelImpL3CachingTests, GivenKernelImpWhenSetSurfaceStateWithUnalignedMemoryThenL3CachingIsDisabled, MatchAny) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<gfxCoreFamily> mockKernel;
    mockKernel.module = module.get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = 0x40;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());
    auto &hwHelper = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily);
    size_t size = hwHelper.getRenderSurfaceStateSize();
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(0x20123);

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    auto expectedSsInHeap = device->getNEODevice()->getBindlessHeapsHelper()->allocateSSInHeap(size, &mockAllocation, NEO::BindlessHeapsHelper::GLOBAL_SSH);

    memset(expectedSsInHeap.ssPtr, 0, size);
    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);
    EXPECT_EQ(mockKernel.getKernelRequiresQueueUncachedMocs(), true);
}

struct MyMockKernel : public Mock<Kernel> {
    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
        setSurfaceStateCalled = true;
    }
    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation) override {
        return KernelImp::setArgBufferWithAlloc(argIndex, argVal, allocation);
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

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc);

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

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc);

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

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc);

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

    mockKernel.setArgBufferWithAlloc(0, 0x1234, &alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
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

        mockKernel.setArgBufferWithAlloc(0, 0x1234, alloc);
        EXPECT_FALSE(mockKernel.getKernelRequiresUncachedMocs());
        context->freeMem(ptr);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct MyMockImage : public WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>> {
    //MyMockImage() : WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>();
    void copySurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset, bool isMediaBlockArg) override {
        passedSurfaceStateHeap = surfaceStateHeap;
        passedSurfaceStateOffset = surfaceStateOffset;
    }
    void *passedSurfaceStateHeap = nullptr;
    uint32_t passedSurfaceStateOffset = 0;
};

HWTEST2_F(SetKernelArg, givenImageAndBindlessKernelWhenSetArgImageThenCopySurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    createKernel();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(neoDevice->getMemoryManager(),
                                                                                                                             neoDevice->getNumGenericSubDevices() > 1,
                                                                                                                             neoDevice->getRootDeviceIndex(),
                                                                                                                             neoDevice->getDeviceBitfield());
    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto &addressingMode = kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode;
    const_cast<NEO::KernelDescriptor::AddressingMode &>(addressingMode) = NEO::KernelDescriptor::Bindless;
    imageArg.bindless = 0x0;
    imageArg.bindful = undefined<SurfaceStateHeapOffset>;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto &hwHelper = NEO::HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto surfaceStateSize = hwHelper.getRenderSurfaceStateSize();

    auto imageHW = std::make_unique<MyMockImage<gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto expectedSsInHeap = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->allocateSSInHeap(surfaceStateSize, imageHW->getAllocation(), BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    kernel->setArgImage(3, sizeof(imageHW.get()), &handle);

    EXPECT_EQ(imageHW->passedSurfaceStateHeap, expectedSsInHeap.ssPtr);
    EXPECT_EQ(imageHW->passedSurfaceStateOffset, 0u);
}

HWTEST2_F(SetKernelArg, givenImageAndBindfulKernelWhenSetArgImageThenCopySurfaceStateToSSHCalledWithCorrectArgs, ImageSupport) {
    createKernel();

    auto &imageArg = const_cast<NEO::ArgDescImage &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[3].template as<NEO::ArgDescImage>());
    auto addressingMode = const_cast<NEO::KernelDescriptor::AddressingMode &>(kernel->kernelImmData->getDescriptor().kernelAttributes.imageAddressingMode);
    addressingMode = NEO::KernelDescriptor::Bindful;
    imageArg.bindless = undefined<CrossThreadDataOffset>;
    imageArg.bindful = 0x40;
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto imageHW = std::make_unique<MyMockImage<gfxCoreFamily>>();
    auto ret = imageHW->initialize(device, &desc);
    auto handle = imageHW->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    kernel->setArgImage(3, sizeof(imageHW.get()), &handle);

    EXPECT_EQ(imageHW->passedSurfaceStateHeap, kernel->getSurfaceStateHeapData());
    EXPECT_EQ(imageHW->passedSurfaceStateOffset, imageArg.bindful);
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct MyMockImageMediaBlock : public WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>> {
    void copySurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset, bool isMediaBlockArg) override {
        isMediaBlockPassedValue = isMediaBlockArg;
    }
    bool isMediaBlockPassedValue = false;
};

HWTEST2_F(SetKernelArg, givenSupportsMediaBlockAndIsMediaBlockImageWhenSetArgImageIsCalledThenIsMediaBlockArgIsPassedCorrectly, ImageSupport) {
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    createKernel();
    auto argIndex = 3u;
    auto &arg = const_cast<NEO::ArgDescriptor &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex]);
    auto imageHW = std::make_unique<MyMockImageMediaBlock<gfxCoreFamily>>();
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
        ModuleFixture::SetUp();

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();

        ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        kernel = L0::Kernel::fromHandle(kernelHandle);
    }

    void TearDown() override {
        Kernel::fromHandle(kernelHandle)->destroy();
        ModuleFixture::TearDown();
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

using KernelWorkDimTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelWorkDimTests, givenGroupCountsWhenPatchingWorkDimThenCrossThreadDataIsPatched) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
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

    kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
    kernel->module = module.get();
    kernel->initialize(&desc);

    EXPECT_FALSE(kernel->printfBuffer == nullptr);
    kernel->printPrintfOutput();
    auto buffer = *reinterpret_cast<uint32_t *>(kernel->printfBuffer->getUnderlyingBuffer());
    EXPECT_EQ(buffer, MyPrintfHandler::getPrintfSurfaceInitialDataSize());
}

using PrintfTest = Test<DeviceFixture>;

TEST_F(PrintfTest, givenKernelWithPrintfThenPrintfBufferIsCreated) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<Kernel> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel.module = &mockModule;

    EXPECT_TRUE(mockKernel.getImmutableData()->getDescriptor().kernelAttributes.flags.usesPrintf);

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "mock";
    mockKernel.createPrintfBuffer();
    EXPECT_NE(nullptr, mockKernel.getPrintfBufferAllocation());
}

TEST_F(PrintfTest, GivenKernelNotUsingPrintfWhenCreatingPrintfBufferThenAllocationIsNotCreated) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<Kernel> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = false;
    mockKernel.module = &mockModule;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "mock";
    mockKernel.createPrintfBuffer();
    EXPECT_EQ(nullptr, mockKernel.getPrintfBufferAllocation());
}

TEST_F(PrintfTest, WhenCreatingPrintfBufferThenAllocationAddedToResidencyContainer) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<Kernel> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel.module = &mockModule;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "mock";
    mockKernel.createPrintfBuffer();

    auto printfBufferAllocation = mockKernel.getPrintfBufferAllocation();
    EXPECT_NE(nullptr, printfBufferAllocation);

    EXPECT_NE(0u, mockKernel.residencyContainer.size());
    EXPECT_EQ(mockKernel.residencyContainer[mockKernel.residencyContainer.size() - 1], printfBufferAllocation);
}

TEST_F(PrintfTest, WhenCreatingPrintfBufferThenCrossThreadDataIsPatched) {
    Mock<Module> mockModule(this->device, nullptr);
    Mock<Kernel> mockKernel;
    mockKernel.descriptor.kernelAttributes.flags.usesPrintf = true;
    mockKernel.module = &mockModule;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "mock";

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

using KernelPatchtokensPrintfStringMapTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelPatchtokensPrintfStringMapTests, givenKernelWithPrintfStringsMapUsageEnabledWhenPrintOutputThenProperStringIsPrinted) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.usesPrintf = true;
    kernelDescriptor->kernelAttributes.flags.usesStringMapForPrintf = true;
    kernelDescriptor->kernelAttributes.binaryFormat = DeviceBinaryFormat::Patchtokens;
    std::string expectedString("test123");
    kernelDescriptor->kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto printfAllocation = reinterpret_cast<uint32_t *>(kernel->getPrintfBufferAllocation()->getUnderlyingBuffer());
    printfAllocation[0] = 8;
    printfAllocation[1] = 0;

    testing::internal::CaptureStdout();
    kernel->printPrintfOutput();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(expectedString.c_str(), output.c_str());
}

TEST_F(KernelPatchtokensPrintfStringMapTests, givenKernelWithPrintfStringsMapUsageDisabledAndNoImplicitArgsWhenPrintOutputThenNothingIsPrinted) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.usesPrintf = true;
    kernelDescriptor->kernelAttributes.flags.usesStringMapForPrintf = false;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.binaryFormat = DeviceBinaryFormat::Patchtokens;
    std::string expectedString("test123");
    kernelDescriptor->kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto printfAllocation = reinterpret_cast<uint32_t *>(kernel->getPrintfBufferAllocation()->getUnderlyingBuffer());
    printfAllocation[0] = 8;
    printfAllocation[1] = 0;

    testing::internal::CaptureStdout();
    kernel->printPrintfOutput();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ("", output.c_str());
}

TEST_F(KernelPatchtokensPrintfStringMapTests, givenKernelWithPrintfStringsMapUsageDisabledAndWithImplicitArgsWhenPrintOutputThenOutputIsPrinted) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.usesPrintf = true;
    kernelDescriptor->kernelAttributes.flags.usesStringMapForPrintf = false;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor->kernelAttributes.binaryFormat = DeviceBinaryFormat::Patchtokens;
    std::string expectedString("test123");
    kernelDescriptor->kernelMetadata.printfStringsMap.insert(std::make_pair(0u, expectedString));

    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    auto printfAllocation = reinterpret_cast<uint32_t *>(kernel->getPrintfBufferAllocation()->getUnderlyingBuffer());
    printfAllocation[0] = 8;
    printfAllocation[1] = 0;

    testing::internal::CaptureStdout();
    kernel->printPrintfOutput();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(expectedString.c_str(), output.c_str());
}

using KernelImplicitArgTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelImplicitArgTests, givenKernelWithImplicitArgsWhenInitializeThenPrintfSurfaceIsCreatedAndProperlyPatchedInImplicitArgs) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesPrintf = false;

    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    auto pImplicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, pImplicitArgs);

    auto printfSurface = kernel->getPrintfBufferAllocation();
    ASSERT_NE(nullptr, printfSurface);

    EXPECT_NE(0u, pImplicitArgs->printfBufferPtr);
    EXPECT_EQ(printfSurface->getGpuAddress(), pImplicitArgs->printfBufferPtr);
}

TEST_F(KernelImplicitArgTests, givenImplicitArgsRequiredWhenCreatingKernelThenImplicitArgsAreCreated) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;

    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    auto pImplicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, pImplicitArgs);

    EXPECT_EQ(sizeof(ImplicitArgs), pImplicitArgs->structSize);
    EXPECT_EQ(0u, pImplicitArgs->structVersion);
}

TEST_F(KernelImplicitArgTests, givenKernelWithImplicitArgsWhenSettingKernelParamsThenImplicitArgsAreUpdated) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    auto simd = mockKernelImmData->kernelDescriptor->kernelAttributes.simdSize;

    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    auto pImplicitArgs = kernel->getImplicitArgs();
    ASSERT_NE(nullptr, pImplicitArgs);

    ImplicitArgs expectedImplicitArgs{sizeof(ImplicitArgs)};
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
    EXPECT_EQ(0, memcmp(pImplicitArgs, &expectedImplicitArgs, sizeof(ImplicitArgs)));
}

using MultiTileModuleTest = Test<MultiTileModuleFixture>;

HWTEST2_F(MultiTileModuleTest, GivenMultiTileDeviceWhenSettingKernelArgAndSurfaceStateThenMultiTileFlagsAreSetCorrectly, IsXeHpCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    WhiteBoxKernelHw<gfxCoreFamily> mockKernel;
    mockKernel.module = modules[0].get();
    mockKernel.initialize(&desc);

    auto &arg = const_cast<NEO::ArgDescPointer &>(mockKernel.kernelImmData->getDescriptor().payloadMappings.explicitArgs[0].template as<NEO::ArgDescPointer>());
    arg.bindless = undefined<CrossThreadDataOffset>;
    arg.bindful = 0x40;

    constexpr size_t size = 128;
    uint64_t gpuAddress = 0x2000;
    char bufferArray[size] = {};
    void *buffer = reinterpret_cast<void *>(bufferArray);
    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);

    mockKernel.setBufferSurfaceState(0, buffer, &mockAllocation);

    void *surfaceStateAddress = ptrOffset(mockKernel.surfaceStateHeapData.get(), arg.bindful);
    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateAddress);
    EXPECT_FALSE(surfaceState->getDisableSupportForMultiGpuAtomics());
    EXPECT_FALSE(surfaceState->getDisableSupportForMultiGpuPartialWrites());
}

} // namespace ult
} // namespace L0

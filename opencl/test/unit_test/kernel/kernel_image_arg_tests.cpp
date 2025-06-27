/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/kernel_arg_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST_F(KernelImageArgTest, GivenKernelWithImageArgsWhenCheckingDifferentScenariosThenBehaviourIsCorrect) {
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageDepth = image->getImageDesc().image_depth;

    cl_mem memObj = image.get();

    pKernel->setArg(0, sizeof(memObj), &memObj);
    pKernel->setArg(1, sizeof(memObj), &memObj);
    pKernel->setArg(3, sizeof(memObj), &memObj);
    pKernel->setArg(4, sizeof(memObj), &memObj);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto imgWidthOffset = ptrOffset(crossThreadData, 0x4);
    EXPECT_EQ(imageWidth, *imgWidthOffset);

    auto imgHeightOffset = ptrOffset(crossThreadData, 0xc);
    EXPECT_EQ(imageHeight, *imgHeightOffset);

    auto dummyOffset = ptrOffset(crossThreadData, 0x20);
    EXPECT_EQ(0x12344321u, *dummyOffset);

    auto imgDepthOffset = ptrOffset(crossThreadData, 0x30);
    EXPECT_EQ(imageDepth, *imgDepthOffset);
}

TEST_F(KernelImageArgTest, givenKernelWithFlatImageTokensWhenArgIsSetThenPatchAllParams) {
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageRowPitch = image->getImageDesc().image_row_pitch;
    uint64_t imageBaseAddress = image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex())->getGpuAddress();

    cl_mem memObj = image.get();

    pKernel->setArg(0, sizeof(memObj), &memObj);
    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto pixelSize = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;

    const auto &metadata = pKernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescImage>().metadataPayload;
    auto offsetFlatBaseOffset = ptrOffset(crossThreadData, metadata.flatBaseOffset);
    EXPECT_EQ(imageBaseAddress, *reinterpret_cast<uint64_t *>(offsetFlatBaseOffset));

    auto offsetFlatWidth = ptrOffset(crossThreadData, metadata.flatWidth);
    EXPECT_EQ(static_cast<uint32_t>((imageWidth * pixelSize) - 1), *offsetFlatWidth);

    auto offsetFlatHeight = ptrOffset(crossThreadData, metadata.flatHeight);
    EXPECT_EQ(static_cast<uint32_t>((imageHeight * pixelSize) - 1), *offsetFlatHeight);

    auto offsetFlatPitch = ptrOffset(crossThreadData, metadata.flatPitch);
    EXPECT_EQ(imageRowPitch - 1, *offsetFlatPitch);
}

TEST_F(KernelImageArgTest, givenKernelWithValidOffsetNumMipLevelsWhenImageArgIsSetThenCrossthreadDataIsProperlyPatched) {
    MockImageBase image;
    image.imageDesc.num_mip_levels = 7U;
    cl_mem imageObj = &image;

    pKernel->setArg(0, sizeof(imageObj), &imageObj);
    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto patchedNumMipLevels = ptrOffset(crossThreadData, offsetNumMipLevelsImage0);
    EXPECT_EQ(7U, *patchedNumMipLevels);
}

TEST_F(KernelImageArgTest, givenImageWithNumSamplesWhenSetArgIsCalledThenPatchNumSamplesInfo) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.num_samples = 16;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, pDevice);
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto sampleImg = Image::create(context.get(), memoryProperties, 0, 0, surfaceFormat, &imgDesc, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem memObj = sampleImg;

    pKernel->setArg(0, sizeof(memObj), &memObj);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto patchedNumSamples = ptrOffset(crossThreadData, 0x3c);
    EXPECT_EQ(16u, *patchedNumSamples);

    sampleImg->release();
}

TEST_F(KernelImageArgTest, givenImageWithWriteOnlyAccessAndReadOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> img(
        Image::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->setAccessQualifier(0, KernelArgMetadata::AccessReadOnly);
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
    retVal = clSetKernelArg(
        pMultiDeviceKernel.get(),
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);

    retVal = clSetKernelArg(
        pMultiDeviceKernel.get(),
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);

    retVal = clSetKernelArg(
        pMultiDeviceKernel.get(),
        1000,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_INDEX);
}

TEST_F(KernelImageArgTest, givenInvalidImageWhenSettingArgImageThenInvalidArgValueErrorIsReturned) {
    cl_mem memObj = reinterpret_cast<cl_mem>(pKernel);

    retVal = pKernel->setArg(0, memObj, 0u);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndWriteOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenReturnsInvalidArgValue) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> img(
        Image::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->setAccessQualifier(0, NEO::KernelArgMetadata::AccessWriteOnly);
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
    Image *image = NULL;
    memObj = image;
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);
    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndReadOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> img(
        Image::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->setAccessQualifier(0, NEO::KernelArgMetadata::AccessReadOnly);
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(KernelImageArgTest, givenImageWithWriteOnlyAccessAndWriteOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> img(
        Image::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->setAccessQualifier(0, NEO::KernelArgMetadata::AccessWriteOnly);
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWTEST_F(KernelImageArgTest, givenImgWithMcsAllocWhenMakeResidentThenMakeMcsAllocationResident) {
    int32_t execStamp = 0;
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, pDevice);
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto img = Image::create(context.get(), memoryProperties, 0, 0, surfaceFormat, &imgDesc, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    img->setMcsAllocation(mcsAlloc);
    cl_mem memObj = img;
    pKernel->setArg(0, sizeof(memObj), &memObj);

    std::unique_ptr<MockCsr<FamilyType>> csr(new MockCsr<FamilyType>(execStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    csr->setupContext(*pDevice->getDefaultEngine().osContext);

    pKernel->makeResident(*csr.get());
    EXPECT_TRUE(csr->isMadeResident(mcsAlloc));

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);

    EXPECT_TRUE(csr->isMadeNonResident(mcsAlloc));

    delete img;
}

TEST_F(KernelImageArgTest, givenKernelWithSettedArgWhenUnSetCalledThenArgIsUnsetAndArgCountIsDecreased) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> img(
        Image::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    cl_mem memObj = img.get();

    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);
    EXPECT_EQ(1u, pKernel->getPatchedArgumentsNum());
    EXPECT_TRUE(pKernel->getKernelArguments()[0].isPatched);
    pKernel->unsetArg(0);
    EXPECT_EQ(0u, pKernel->getPatchedArgumentsNum());
    EXPECT_FALSE(pKernel->getKernelArguments()[0].isPatched);
}

TEST_F(KernelImageArgTest, givenNullKernelWhenClSetKernelArgCalledThenInvalidKernelCodeReturned) {
    cl_mem memObj = NULL;
    retVal = clSetKernelArg(
        NULL,
        1000,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL);
}

class MockSharingHandler : public SharingHandler {
  public:
    void synchronizeObject(UpdateData &updateData) override {
        updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
    }
};

TEST_F(KernelImageArgTest, givenKernelWithSharedImageWhenSetArgCalledThenUsingSharedObjArgsShouldBeTrue) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        0, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> img(
        Image::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    cl_mem memObj = img.get();

    MockSharingHandler *mockSharingHandler = new MockSharingHandler;
    img->setSharingHandler(mockSharingHandler);

    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, pKernel->getPatchedArgumentsNum());
    EXPECT_TRUE(pKernel->getKernelArguments()[0].isPatched);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());
}

class KernelImageArgTestBindless : public KernelImageArgTest {
  public:
    void SetUp() override {
        debugManager.flags.UseBindlessMode.set(1);
        KernelImageArgTest::SetUp();

        auto &img = pKernelInfo->argAsImg(0);
        img.bindful = undefined<SurfaceStateHeapOffset>;
        img.bindless = bindlessOffset;

        pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();
    }
    void TearDown() override {
        KernelImageArgTest::TearDown();
    }
    DebugManagerStateRestore restorer;
    const CrossThreadDataOffset bindlessOffset = 0x0;
};

HWTEST_F(KernelImageArgTestBindless, givenUsedBindlessImagesWhenSettingKernelArgThenOffsetInCrossThreadDataIsNotPatched) {
    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(), bindlessOffset));
    *patchLocation = 0xdead;

    cl_mem memObj = image.get();
    pKernel->setArg(0, sizeof(memObj), &memObj);

    EXPECT_EQ(0xdeadu, *patchLocation);
}

HWTEST_F(KernelImageArgTestBindless, givenUsedBindlessImagesWhenSettingKernelArgThenSurfaceStateIsSet) {
    cl_mem memObj = image.get();
    pKernel->setArg(0, sizeof(memObj), &memObj);

    const auto &gfxCoreHelper = pKernel->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  ssOffset));
    const auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    NEO::SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);
    const auto expectedBaseAddress = image->getMultiGraphicsAllocation().getGraphicsAllocation(pDevice->getRootDeviceIndex())->getGpuAddress() + surfaceOffsets.offset;

    EXPECT_EQ(expectedBaseAddress, surfaceAddress);
}

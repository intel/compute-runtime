/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/kernel/kernel.h"
#include "test.h"
#include "unit_tests/fixtures/kernel_arg_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_image.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST_F(KernelImageArgTest, GIVENkernelWithImageArgsWHENcheckDifferentScenariosTHENproperBehaviour) {
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageDepth = image->getImageDesc().image_depth;
    uint32_t objectId = pKernelInfo->kernelArgInfo[4].offsetHeap;

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

    EXPECT_EQ(objectId, *crossThreadData);
}

TEST_F(KernelImageArgTest, givenKernelWithFlatImageTokensWhenArgIsSetThenPatchAllParams) {
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageRowPitch = image->getImageDesc().image_row_pitch;
    uint64_t imageBaseAddress = image->getGraphicsAllocation()->getGpuAddress();

    cl_mem memObj = image.get();

    pKernel->setArg(0, sizeof(memObj), &memObj);
    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto pixelSize = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;

    auto offsetFlatBaseOffset = ptrOffset(crossThreadData, pKernel->getKernelInfo().kernelArgInfo[0].offsetFlatBaseOffset);
    EXPECT_EQ(imageBaseAddress, *reinterpret_cast<uint64_t *>(offsetFlatBaseOffset));

    auto offsetFlatWidth = ptrOffset(crossThreadData, pKernel->getKernelInfo().kernelArgInfo[0].offsetFlatWidth);
    EXPECT_EQ(static_cast<uint32_t>((imageWidth * pixelSize) - 1), *offsetFlatWidth);

    auto offsetFlatHeight = ptrOffset(crossThreadData, pKernel->getKernelInfo().kernelArgInfo[0].offsetFlatHeight);
    EXPECT_EQ(static_cast<uint32_t>((imageHeight * pixelSize) - 1), *offsetFlatHeight);

    auto offsetFlatPitch = ptrOffset(crossThreadData, pKernel->getKernelInfo().kernelArgInfo[0].offsetFlatPitch);
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

    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    auto sampleImg = Image::create(context.get(), {}, 0, 0, surfaceFormat, &imgDesc, nullptr, retVal);
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_READ_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
    retVal = clSetKernelArg(
        pKernel.get(),
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);

    retVal = clSetKernelArg(
        pKernel.get(),
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);

    retVal = clSetKernelArg(
        pKernel.get(),
        1000,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_INDEX);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndWriteOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenReturnsInvalidArgValue) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_WRITE_ONLY;
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_READ_ONLY;
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_WRITE_ONLY;
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

    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    auto img = Image::create(context.get(), {}, 0, 0, surfaceFormat, &imgDesc, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    img->setMcsAllocation(mcsAlloc);
    cl_mem memObj = img;
    pKernel->setArg(0, sizeof(memObj), &memObj);

    std::unique_ptr<MockCsr<FamilyType>> csr(new MockCsr<FamilyType>(execStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex()));
    csr->setupContext(*pDevice->getDefaultEngine().osContext);

    pKernel->makeResident(*csr.get());
    EXPECT_TRUE(csr->isMadeResident(mcsAlloc));

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());

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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imgDesc, nullptr, retVal));
    cl_mem memObj = img.get();

    MockSharingHandler *mockSharingHandler = new MockSharingHandler;
    img->setSharingHandler(mockSharingHandler);

    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, pKernel->getPatchedArgumentsNum());
    EXPECT_TRUE(pKernel->getKernelArguments()[0].isPatched);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());
}

TEST_F(KernelImageArgTest, givenWritableImageWhenSettingAsArgThenDoNotExpectAllocationInCacheFlushVector) {
    MockImageBase image;
    image.graphicsAllocation->setMemObjectsAllocationWithWritableFlags(true);
    image.graphicsAllocation->setFlushL3Required(false);

    cl_mem imageObj = &image;

    pKernel->setArg(0, sizeof(imageObj), &imageObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pKernel->kernelArgRequiresCacheFlush[0]);
}

TEST_F(KernelImageArgTest, givenCacheFlushImageWhenSettingAsArgThenExpectAllocationInCacheFlushVector) {
    MockImageBase image;
    image.graphicsAllocation->setMemObjectsAllocationWithWritableFlags(false);
    image.graphicsAllocation->setFlushL3Required(true);

    cl_mem imageObj = &image;

    pKernel->setArg(0, sizeof(imageObj), &imageObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(image.graphicsAllocation, pKernel->kernelArgRequiresCacheFlush[0]);
}

TEST_F(KernelImageArgTest, givenNoCacheFlushImageWhenSettingAsArgThenExpectAllocationInCacheFlushVector) {
    MockImageBase image;
    image.graphicsAllocation->setMemObjectsAllocationWithWritableFlags(false);
    image.graphicsAllocation->setFlushL3Required(false);

    cl_mem imageObj = &image;

    pKernel->setArg(0, sizeof(imageObj), &imageObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pKernel->kernelArgRequiresCacheFlush[0]);
}

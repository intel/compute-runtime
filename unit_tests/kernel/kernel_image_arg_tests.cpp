/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_csr.h"
#include "gtest/gtest.h"

using namespace OCLRT;

class KernelImageArgTest : public Test<DeviceFixture> {
  public:
    KernelImageArgTest() {
    }

  protected:
    void SetUp() override {
        pKernelInfo = KernelInfo::create();
        KernelArgPatchInfo kernelArgPatchInfo;

        kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;

        pKernelInfo->kernelArgInfo.resize(5);
        pKernelInfo->kernelArgInfo[4].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].offsetImgWidth = 0x4;
        pKernelInfo->kernelArgInfo[0].offsetNumSamples = 0x3c;
        pKernelInfo->kernelArgInfo[1].offsetImgHeight = 0xc;
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].size = sizeof(void *);
        pKernelInfo->kernelArgInfo[3].offsetImgDepth = 0x30;
        pKernelInfo->kernelArgInfo[4].offsetHeap = 0x20;
        pKernelInfo->kernelArgInfo[4].offsetObjectId = 0x0;

        pKernelInfo->kernelArgInfo[4].isImage = true;
        pKernelInfo->kernelArgInfo[3].isImage = true;
        pKernelInfo->kernelArgInfo[2].isImage = true;
        pKernelInfo->kernelArgInfo[1].isImage = true;
        pKernelInfo->kernelArgInfo[0].isImage = true;

        DeviceFixture::SetUp();
        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(2, &Kernel::setArgImmediate);
        pKernel->setKernelArgHandler(3, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(4, &Kernel::setArgImage);

        uint32_t crossThreadData[0x40] = {};
        crossThreadData[0x20 / sizeof(uint32_t)] = 0x12344321;
        pKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

        context = new MockContext(pDevice);
        image = Image2dHelper<>::create(context);
        ASSERT_NE(nullptr, image);
    }

    void TearDown() override {
        delete pKernelInfo;
        delete pKernel;
        delete image;
        delete context;
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram program;
    MockKernel *pKernel = nullptr;
    KernelInfo *pKernelInfo;

    MockContext *context;
    Image *image;
    SKernelBinaryHeaderCommon kernelHeader;
    char surfaceStateHeap[0x80];
};

TEST_F(KernelImageArgTest, GIVENkernelWithImageArgsWHENcheckDifferentScenariosTHENproperBehaviour) {
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageDepth = image->getImageDesc().image_depth;
    uint32_t objectId = pKernelInfo->kernelArgInfo[4].offsetHeap;

    cl_mem memObj = image;

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

TEST_F(KernelImageArgTest, givenImageWithNumSamplesWhenSetArgIsCalledThenPatchNumSamplesInfo) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.num_samples = 16;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    auto sampleImg = Image::create(context, 0, surfaceFormat, &imgDesc, nullptr, retVal);
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
    std::unique_ptr<Image> img(Image::create(context, flags, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_READ_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL_ARGS);
    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL_ARGS);

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL_ARGS);

    retVal = clSetKernelArg(
        pKernel,
        1000,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_INDEX);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndWriteOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context, flags, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_WRITE_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL_ARGS);
    Image *image = NULL;
    memObj = image;
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);
    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndReadOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context, flags, surfaceFormat, &imgDesc, nullptr, retVal));
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
    std::unique_ptr<Image> img(Image::create(context, flags, surfaceFormat, &imgDesc, nullptr, retVal));
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
    auto img = Image::create(context, 0, surfaceFormat, &imgDesc, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemory(4096);
    img->setMcsAllocation(mcsAlloc);
    cl_mem memObj = img;
    pKernel->setArg(0, sizeof(memObj), &memObj);

    std::unique_ptr<OsAgnosticMemoryManager> memoryManager(new OsAgnosticMemoryManager());
    std::unique_ptr<MockCsr<FamilyType>> csr(new MockCsr<FamilyType>(execStamp));
    csr->setMemoryManager(memoryManager.get());

    pKernel->makeResident(*csr.get());
    EXPECT_TRUE(csr->isMadeResident(mcsAlloc));

    csr->makeSurfacePackNonResident(nullptr);

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
    std::unique_ptr<Image> img(Image::create(context, flags, surfaceFormat, &imgDesc, nullptr, retVal));
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

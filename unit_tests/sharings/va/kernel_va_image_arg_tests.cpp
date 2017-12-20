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

#include "runtime/sharings/va/va_surface.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/sharings/va/mock_va_sharing.h"
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

TEST_F(KernelImageArgTest, givenSharedImageWhenSetArgIsCalledThenReportSharedObjUsage) {
    MockVaSharing vaSharing;
    VASurfaceID vaSurfaceId = 0u;
    vaSharing.updateAcquiredHandle(1u);
    std::unique_ptr<Image> sharedImage(VASurface::createSharedVaSurface(context, &vaSharing.m_sharingFunctions,
                                                                        CL_MEM_READ_WRITE, &vaSurfaceId, 0, nullptr));

    auto sharedMem = static_cast<cl_mem>(sharedImage.get());
    auto nonSharedMem = static_cast<cl_mem>(image);

    EXPECT_FALSE(pKernel->isUsingSharedObjArgs());
    this->pKernel->setArg(0, sizeof(cl_mem *), &nonSharedMem);
    EXPECT_FALSE(pKernel->isUsingSharedObjArgs());

    this->pKernel->setArg(0, sizeof(cl_mem *), &sharedMem);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());
}

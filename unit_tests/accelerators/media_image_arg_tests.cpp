/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "hw_cmds.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/surface.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

using namespace OCLRT;

class MediaImageSetArgTest : public DeviceFixture,
                             public testing::Test {
  public:
    MediaImageSetArgTest()

    {
        memset(&kernelHeader, 0, sizeof(kernelHeader));
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        pKernelInfo = KernelInfo::create();
        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());

        kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;
        pKernelInfo->isVmeWorkload = true;

        pKernelInfo->kernelArgInfo.resize(2);
        pKernelInfo->kernelArgInfo[1].offsetHeap = 0x00;
        pKernelInfo->kernelArgInfo[0].offsetHeap = 0x40;

        pKernelInfo->kernelArgInfo[1].isMediaImage = true;
        pKernelInfo->kernelArgInfo[0].isMediaImage = true;

        pKernelInfo->kernelArgInfo[1].isImage = true;
        pKernelInfo->kernelArgInfo[0].isImage = true;

        pKernel = new MockKernel(program.get(), *pKernelInfo, *pDevice);
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        ASSERT_EQ(true, pKernel->isVmeKernel());

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
        context = new MockContext(pDevice);
        srcImage = Image2dHelper<>::create(context);
        ASSERT_NE(nullptr, srcImage);
    }

    void TearDown() override {
        delete srcImage;
        delete pKernel;
        delete pKernelInfo;
        delete context;
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    std::unique_ptr<MockProgram> program;
    MockKernel *pKernel = nullptr;
    SKernelBinaryHeaderCommon kernelHeader;
    KernelInfo *pKernelInfo = nullptr;
    char surfaceStateHeap[0x80];
    Image *srcImage = nullptr;
};

HWTEST_F(MediaImageSetArgTest, setKernelArgImage) {
    typedef typename FamilyType::MEDIA_SURFACE_STATE MEDIA_SURFACE_STATE;

    auto pSurfaceState = reinterpret_cast<const MEDIA_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    srcImage->setMediaImageArg(const_cast<MEDIA_SURFACE_STATE *>(pSurfaceState));

    SurfaceOffsets surfaceOffsets;
    srcImage->getSurfaceOffsets(surfaceOffsets);

    EXPECT_EQ(srcImage->getGraphicsAllocation()->getGpuAddress() + surfaceOffsets.offset,
              pSurfaceState->getSurfaceBaseAddress());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(MediaImageSetArgTest, clSetKernelArgImage) {
    typedef typename FamilyType::MEDIA_SURFACE_STATE MEDIA_SURFACE_STATE;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pSurfaceState = reinterpret_cast<const MEDIA_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    void *surfaceAddress = reinterpret_cast<void *>(pSurfaceState->getSurfaceBaseAddress());
    ASSERT_EQ(srcImage->getCpuAddress(), surfaceAddress);
    EXPECT_EQ(srcImage->getImageDesc().image_width, pSurfaceState->getWidth());
    EXPECT_EQ(srcImage->getImageDesc().image_height, pSurfaceState->getHeight());

    typename FamilyType::MEDIA_SURFACE_STATE::TILE_MODE tileMode;

    if (srcImage->allowTiling()) {
        tileMode = FamilyType::MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_YMAJOR;
    } else {
        tileMode = FamilyType::MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_LINEAR;
    }

    EXPECT_EQ(tileMode, pSurfaceState->getTileMode());
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM_VA, pSurfaceState->getSurfaceFormat());

    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE, pSurfaceState->getPictureStructure());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);

    for (auto &surface : surfaces) {
        delete surface;
    }

    EXPECT_EQ(1u, surfaces.size());
}

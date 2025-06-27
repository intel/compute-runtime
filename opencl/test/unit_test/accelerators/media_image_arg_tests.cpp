/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/surface.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

namespace NEO {
struct KernelInfo;
} // namespace NEO

using namespace NEO;

class MediaImageSetArgTest : public ClDeviceFixture,
                             public testing::Test {
  public:
    MediaImageSetArgTest() = default;

  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
        context = new MockContext(pClDevice);

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
        program->setContext(context);

        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;

        pKernelInfo->addArgImage(0, 0x00, iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA);
        pKernelInfo->addArgImage(0, 0x40, iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA);

        int32_t retVal = CL_INVALID_PLATFORM;
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(program.get(), MockKernel::toKernelInfoContainer(*static_cast<KernelInfo *>(pKernelInfo.get()), rootDeviceIndex), retVal);
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        ASSERT_EQ(true, pKernel->isVmeKernel());

        pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImage);

        srcImage = Image2dHelperUlt<>::create(context);
        ASSERT_NE(nullptr, srcImage);
    }

    void TearDown() override {
        delete srcImage;
        delete pMultiDeviceKernel;
        program.reset();
        delete context;
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    std::unique_ptr<MockProgram> program;
    MockKernel *pKernel = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char surfaceStateHeap[0x80];
    Image *srcImage = nullptr;
};

HWTEST_F(MediaImageSetArgTest, WhenSettingMediaImageArgThenArgsSetCorrectly) {
    typedef typename FamilyType::MEDIA_SURFACE_STATE MEDIA_SURFACE_STATE;

    auto pSurfaceState = reinterpret_cast<const MEDIA_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    srcImage->setMediaImageArg(const_cast<MEDIA_SURFACE_STATE *>(pSurfaceState), pClDevice->getRootDeviceIndex());

    SurfaceOffsets surfaceOffsets;
    srcImage->getSurfaceOffsets(surfaceOffsets);

    EXPECT_EQ(srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress() + surfaceOffsets.offset,
              pSurfaceState->getSurfaceBaseAddress());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(0u, surfaces.size());
}

HWTEST_F(MediaImageSetArgTest, WhenSettingKernelArgImageThenArgsSetCorrectly) {
    typedef typename FamilyType::MEDIA_SURFACE_STATE MEDIA_SURFACE_STATE;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pSurfaceState = reinterpret_cast<const MEDIA_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsImg(0).bindful));

    uint64_t surfaceAddress = pSurfaceState->getSurfaceBaseAddress();
    ASSERT_EQ(srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), surfaceAddress);
    EXPECT_EQ(srcImage->getImageDesc().image_width, pSurfaceState->getWidth());
    EXPECT_EQ(srcImage->getImageDesc().image_height, pSurfaceState->getHeight());

    typename FamilyType::MEDIA_SURFACE_STATE::TILE_MODE tileMode;

    if (srcImage->isTiledAllocation()) {
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

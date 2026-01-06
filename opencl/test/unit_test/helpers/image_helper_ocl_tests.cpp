/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/image_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"

#include "opencl/test/unit_test/command_queue/enqueue_copy_image_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

HWTEST_F(EnqueueCopyImageTest, whenImagesCheckedForPackageFormatThenFalseIsReturned) {
    std::unique_ptr<ImageHw<FamilyType>> srcImage1(static_cast<ImageHw<FamilyType> *>(ImageHelperUlt<Image2dDefaults>::create(context)));
    std::unique_ptr<ImageHw<FamilyType>> dstImage1(static_cast<ImageHw<FamilyType> *>(ImageHelperUlt<Image2dDefaults>::create(context)));
    auto srcAllocation1 = srcImage1->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    auto dstAllocation1 = dstImage1->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    auto srcMockGmmResourceInfo1 = reinterpret_cast<MockGmmResourceInfo *>(srcAllocation1->getDefaultGmm()->gmmResourceInfo.get());
    auto dstMockGmmResourceInfo1 = reinterpret_cast<MockGmmResourceInfo *>(dstAllocation1->getDefaultGmm()->gmmResourceInfo.get());
    srcMockGmmResourceInfo1->getResourceFlags()->Info.Tile64 = 1;
    dstMockGmmResourceInfo1->getResourceFlags()->Info.Tile64 = 1;

    auto srcImageDescriptor1 = Image::convertDescriptor(srcImage1->getImageDesc());
    ClSurfaceFormatInfo srcSurfaceFormatInfo1 = srcImage1->getSurfaceFormatInfo();
    SurfaceOffsets srcSurfaceOffsets1;
    srcImage1->getSurfaceOffsets(srcSurfaceOffsets1);

    ImageInfo srcImgInfo1;
    srcImgInfo1.imgDesc = srcImageDescriptor1;
    srcImgInfo1.surfaceFormat = &srcSurfaceFormatInfo1.surfaceFormat;
    srcImgInfo1.xOffset = srcSurfaceOffsets1.xOffset;

    auto dstImageDescriptor1 = Image::convertDescriptor(dstImage1->getImageDesc());
    ClSurfaceFormatInfo dstSurfaceFormatInfo1 = dstImage1->getSurfaceFormatInfo();
    SurfaceOffsets dstSurfaceOffsets1;
    dstImage1->getSurfaceOffsets(dstSurfaceOffsets1);

    ImageInfo dstImgInfo1;
    dstImgInfo1.imgDesc = dstImageDescriptor1;
    dstImgInfo1.surfaceFormat = &dstSurfaceFormatInfo1.surfaceFormat;
    dstImgInfo1.xOffset = dstSurfaceOffsets1.xOffset;

    EXPECT_FALSE(ImageHelper::areImagesCompatibleWithPackedFormat(context->getDevice(0)->getProductHelper(), srcImgInfo1, dstImgInfo1, srcAllocation1, dstAllocation1, 4));
}

HWTEST_F(EnqueueCopyImageTest, givenPackedSurfaceStateWhenCopyingImageThenSurfaceStateIsNotModified) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    typedef typename RENDER_SURFACE_STATE::SURFACE_FORMAT SURFACE_FORMAT;

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    mockCmdQ->clearBcsEngines();

    std::unique_ptr<ImageHw<FamilyType>> srcImage(static_cast<ImageHw<FamilyType> *>(Image2dHelperUlt<>::create(context)));
    std::unique_ptr<ImageHw<FamilyType>> dstImage(static_cast<ImageHw<FamilyType> *>(Image2dHelperUlt<>::create(context)));
    auto srcMockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo.get());
    auto dstMockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(dstImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo.get());
    srcMockGmmResourceInfo->getResourceFlags()->Info.Tile64 = 1;
    dstMockGmmResourceInfo->getResourceFlags()->Info.Tile64 = 1;

    auto retVal = EnqueueCopyImageHelper<>::enqueueCopyImage(
        pCmdQ,
        srcImage.get(),
        dstImage.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
    parseCommands<FamilyType>(*pCmdQ);

    for (uint32_t i = 0; i < 2; ++i) {
        const auto surfaceState = SurfaceStateAccessor::getSurfaceState<FamilyType>(mockCmdQ, i);

        EXPECT_EQ(SURFACE_FORMAT::SURFACE_FORMAT_R32_UINT, surfaceState->getSurfaceFormat());

        const auto &imageDesc = dstImage->getImageDesc();

        EXPECT_EQ(7u, surfaceState->getWidth());
        EXPECT_EQ(imageDesc.image_height, surfaceState->getHeight());
    }

    const auto srcSurfaceState = SurfaceStateAccessor::getSurfaceState<FamilyType>(mockCmdQ, 0);
    EXPECT_EQ(srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), srcSurfaceState->getSurfaceBaseAddress());

    const auto dstSurfaceState = SurfaceStateAccessor::getSurfaceState<FamilyType>(mockCmdQ, 1);
    EXPECT_EQ(dstImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), dstSurfaceState->getSurfaceBaseAddress());
}

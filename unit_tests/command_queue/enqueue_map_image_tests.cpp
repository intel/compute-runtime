/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/user_event.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

struct EnqueueMapImageTest : public DeviceFixture,
                             public CommandQueueHwFixture,
                             public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueMapImageTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pDevice, 0);
        context = new MockContext(pDevice);
        image = ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context);
    }

    void TearDown() override {
        delete image;
        context->release();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    MockContext *context;
    cl_int retVal = CL_INVALID_VALUE;
    Image *image = nullptr;
    char srcMemory[128];
};

struct EnqueueMapImageParamsTest : public EnqueueMapImageTest,
                                   public ::testing::WithParamInterface<uint32_t> {
};

TEST_F(EnqueueMapImageTest, reuseMappedPtrForTiledImg) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};

    auto mapAllocation = image->getMapAllocation();
    EXPECT_NE(nullptr, mapAllocation);

    auto ptr1 = pCmdQ->enqueueMapImage(
        image, true, mapFlags, origin,
        region, nullptr, nullptr, 0,
        nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image->getHostPtr());
    mapAllocation = image->getMapAllocation();
    EXPECT_NE(nullptr, mapAllocation);

    auto ptr2 = pCmdQ->enqueueMapImage(
        image, true, mapFlags, origin,
        region, nullptr, nullptr, 0,
        nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(ptr1, ptr2);

    retVal = pCmdQ->enqueueUnmapMemObject(image, ptr1, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueMapImageTest, givenAllocatedMapPtrAndMapWithDifferentOriginIsCalledThenReturnDifferentPointers) {
    std::unique_ptr<Image> img(Image2dHelper<Image2dDefaults>::create(context));
    auto mapFlags = CL_MAP_READ;
    const size_t origin1[3] = {0, 0, 0};
    const size_t origin2[3] = {2, 2, 0};
    const size_t region[3] = {1, 1, 1};

    auto ptr1 = pCmdQ->enqueueMapImage(img.get(), true, mapFlags, origin1,
                                       region, nullptr, nullptr, 0,
                                       nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto ptr2 = pCmdQ->enqueueMapImage(img.get(), true, mapFlags, origin2,
                                       region, nullptr, nullptr, 0,
                                       nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(ptr1, ptr2);
    EXPECT_NE(nullptr, img->getAllocatedMapPtr());

    size_t mapOffset = img->getSurfaceFormatInfo().ImageElementSizeInBytes * origin2[0] +
                       img->getHostPtrRowPitch() * origin2[1];
    EXPECT_EQ(ptr2, ptrOffset(ptr1, mapOffset));
}

typedef EnqueueMapImageParamsTest MipMapMapImageParamsTest;

TEST_P(MipMapMapImageParamsTest, givenAllocatedMapPtrWhenMapsWithDifferentMipMapsAreCalledThenReturnDifferentPointers) {
    auto image_type = (cl_mem_object_type)GetParam();
    cl_int retVal = CL_SUCCESS;
    cl_image_desc imageDesc = {};
    imageDesc.image_type = image_type;
    imageDesc.num_mip_levels = 10;
    imageDesc.image_width = 4;
    imageDesc.image_height = 1;
    imageDesc.image_depth = 1;
    const size_t origin1[4] = {0, 0, 0, 0};
    size_t origin2[4] = {0, 0, 0, 0};
    std::unique_ptr<Image> image;
    size_t mapOffset = 16u;
    switch (image_type) {
    case CL_MEM_OBJECT_IMAGE1D:
        origin2[1] = 1;
        image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        origin2[2] = 1;
        imageDesc.image_array_size = 2;
        image = std::unique_ptr<Image>(ImageHelper<Image1dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        origin2[2] = 1;
        image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        origin2[3] = 1;
        imageDesc.image_array_size = 2;
        image = std::unique_ptr<Image>(ImageHelper<Image2dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        origin2[3] = 1;
        image = std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(context, &imageDesc));
        break;
    }
    EXPECT_NE(nullptr, image.get());

    auto mapFlags = CL_MAP_READ;
    const size_t region[3] = {1, 1, 1};

    auto ptr1 = pCmdQ->enqueueMapImage(image.get(), true, mapFlags, origin1,
                                       region, nullptr, nullptr, 0,
                                       nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto ptr2 = pCmdQ->enqueueMapImage(image.get(), true, mapFlags, origin2,
                                       region, nullptr, nullptr, 0,
                                       nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(ptr1, ptr2);
    if (image->mappingOnCpuAllowed() == false) {
        EXPECT_NE(nullptr, image->getAllocatedMapPtr());
    }

    EXPECT_EQ(ptr2, ptrOffset(ptr1, mapOffset));
}

INSTANTIATE_TEST_CASE_P(MipMapMapImageParamsTest_givenAllocatedMapPtrAndMapWithDifferentMipMapsIsCalledThenReturnDifferentPointers,
                        MipMapMapImageParamsTest, ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));

template <typename GfxFamily>
struct mockedImage : public ImageHw<GfxFamily> {
    using ImageHw<GfxFamily>::ImageHw;
    void setAllocatedMapPtr(void *allocatedMapPtr) override {
        ownershipTaken = this->hasOwnership();
        MemObj::setAllocatedMapPtr(allocatedMapPtr);
    }
    bool ownershipTaken = false;
};

HWTEST_F(EnqueueMapImageTest, givenTiledImageWhenMapImageIsCalledThenStorageIsSetWithImageMutexTaken) {
    auto imageFormat = image->getImageFormat();
    auto imageDesc = image->getImageDesc();
    auto graphicsAllocation = image->getGraphicsAllocation();
    auto surfaceFormatInfo = image->getSurfaceFormatInfo();

    mockedImage<FamilyType> mockImage(context,
                                      0,
                                      4096u,
                                      nullptr,
                                      imageFormat,
                                      imageDesc,
                                      false,
                                      graphicsAllocation,
                                      true,
                                      0,
                                      0,
                                      surfaceFormatInfo,
                                      nullptr);

    mockImage.createFunction = image->createFunction;

    auto mapAllocation = mockImage.getMapAllocation();
    EXPECT_EQ(nullptr, mapAllocation);
    EXPECT_EQ(nullptr, mockImage.getHostPtr());

    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};

    auto apiMapPtr = pCmdQ->enqueueMapImage(
        &mockImage, true, mapFlags, origin,
        region, nullptr, nullptr, 0,
        nullptr, nullptr, retVal);
    EXPECT_TRUE(mockImage.ownershipTaken);

    auto mapPtr = mockImage.getAllocatedMapPtr();
    EXPECT_EQ(apiMapPtr, mapPtr);
    mapAllocation = mockImage.getMapAllocation();
    EXPECT_NE(nullptr, mapAllocation);
    EXPECT_EQ(apiMapPtr, mapAllocation->getUnderlyingBuffer());

    auto osContextId = pCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getContextId();
    auto expectedTaskCount = pCmdQ->getGpgpuCommandStreamReceiver().peekTaskCount();
    auto actualMapAllocationTaskCount = mapAllocation->getTaskCount(osContextId);
    EXPECT_EQ(expectedTaskCount, actualMapAllocationTaskCount);

    pDevice->getMemoryManager()->freeGraphicsMemory(mockImage.getMapAllocation());
    mockImage.releaseAllocatedMapPtr();
}

TEST_F(EnqueueMapImageTest, checkPointer) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    auto ptr = pCmdQ->enqueueMapImage(
        image,
        true,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        retVal);
    if (image->isMemObjZeroCopy()) {
        EXPECT_EQ(image->getCpuAddress(), ptr);
    } else {
        EXPECT_NE(image->getCpuAddress(), ptr);
    }
    size_t imageRowPitchRef = 0;
    image->getImageInfo(CL_IMAGE_ROW_PITCH, sizeof(imageRowPitchRef), &imageRowPitchRef, nullptr);
    EXPECT_EQ(imageRowPitch, imageRowPitchRef);

    size_t imageSlicePitchRef = 0;
    image->getImageInfo(CL_IMAGE_SLICE_PITCH, sizeof(imageSlicePitchRef), &imageSlicePitchRef, nullptr);
    EXPECT_EQ(imageSlicePitch, imageSlicePitchRef);
}

TEST_F(EnqueueMapImageTest, checkRetVal) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    auto ptr = pCmdQ->enqueueMapImage(
        image,
        true,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        retVal);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t imageRowPitchRef = 0;
    image->getImageInfo(CL_IMAGE_ROW_PITCH, sizeof(imageRowPitchRef), &imageRowPitchRef, nullptr);
    EXPECT_EQ(imageRowPitch, imageRowPitchRef);

    size_t imageSlicePitchRef = 0;
    image->getImageInfo(CL_IMAGE_SLICE_PITCH, sizeof(imageSlicePitchRef), &imageSlicePitchRef, nullptr);
    EXPECT_EQ(imageSlicePitch, imageSlicePitchRef);
}

TEST_F(EnqueueMapImageTest, givenNonReadOnlyMapWithOutEventWhenMappedThenSetEventAndIncraseTaskCountFromWriteImage) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    cl_event mapEventReturned = nullptr;
    cl_event unmapEventReturned = nullptr;
    uint32_t tagHW = 0;
    auto mapFlags = CL_MAP_WRITE;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    size_t GWS = 1;

    MockKernelWithInternals kernel(*pDevice);
    *pTagMemory = tagHW;
    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    auto tag_address = commandStreamReceiver.getTagAddress();
    EXPECT_TRUE(pTagMemory == tag_address);

    struct E2Clb {
        static void CL_CALLBACK SignalEv2(cl_event e, cl_int status, void *data) {
            uint32_t *pTagMem = static_cast<uint32_t *>(data);
            *pTagMem = 4;
        }
    };

    uint32_t taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(1u, taskCount);

    // enqueue something that can be finished...
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, 0, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    *pTagMemory = tagHW += 3;
    auto ptr = pCmdQ->enqueueMapImage(
        image,
        false,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        &mapEventReturned,
        retVal);

    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto mapEvent = castToObject<Event>(mapEventReturned);
    EXPECT_TRUE(CL_COMMAND_MAP_IMAGE == mapEvent->getCommandType());

    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(3u, taskCount);

    clSetEventCallback(mapEventReturned, CL_COMPLETE, E2Clb::SignalEv2, (void *)pTagMemory);

    retVal = clWaitForEvents(1, &mapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(4u, *pTagMemory);
    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(3u, taskCount);

    (*pTagMemory)++;
    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        image,
        ptr,
        0,
        nullptr,
        &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto unmapEvent = castToObject<Event>(unmapEventReturned);
    EXPECT_TRUE(CL_COMMAND_UNMAP_MEM_OBJECT == unmapEvent->getCommandType());

    retVal = clWaitForEvents(1, &unmapEventReturned);

    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(4u, taskCount);

    clReleaseEvent(mapEventReturned);
    clReleaseEvent(unmapEventReturned);
}

TEST_F(EnqueueMapImageTest, givenReadOnlyMapWithOutEventWhenMappedThenSetEventAndDontIncraseTaskCountFromWriteImage) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    cl_event mapEventReturned = nullptr;
    cl_event unmapEventReturned = nullptr;
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    *pTagMemory = 5;

    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();

    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    auto ptr = pCmdQ->enqueueMapImage(image, false, mapFlags, origin, region, nullptr, nullptr, 0,
                                      nullptr, &mapEventReturned, retVal);

    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, commandStreamReceiver.peekTaskCount());

    auto mapEvent = castToObject<Event>(mapEventReturned);
    EXPECT_TRUE(CL_COMMAND_MAP_IMAGE == mapEvent->getCommandType());

    retVal = clWaitForEvents(1, &mapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(pCmdQ, image, ptr, 0, nullptr, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(2u, commandStreamReceiver.peekTaskCount());

    auto unmapEvent = castToObject<Event>(unmapEventReturned);
    EXPECT_TRUE(CL_COMMAND_UNMAP_MEM_OBJECT == unmapEvent->getCommandType());

    retVal = clWaitForEvents(1, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(mapEventReturned);
    clReleaseEvent(unmapEventReturned);
}

HWTEST_F(EnqueueMapImageTest, MapImageEventProperties) {
    cl_event eventReturned = nullptr;
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    uint32_t forceTaskCount = 100;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskCount = forceTaskCount;

    auto ptr = clEnqueueMapImage(
        pCmdQ,
        image,
        CL_FALSE,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        &eventReturned,
        &retVal);

    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, eventReturned);

    auto eventObject = castToObject<Event>(eventReturned);
    EXPECT_EQ(forceTaskCount + 1, eventObject->peekTaskCount());
    EXPECT_TRUE(eventObject->updateStatusAndCheckCompletion());

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        image,
        ptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(eventReturned);
}

HWTEST_F(EnqueueMapImageTest, givenZeroCopyImageWhenItIsMappedAndReturnsEventThenEventHasCorrectProperties) {
    cl_event eventReturned = nullptr;
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    uint32_t forceTaskCount = 100;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskCount = forceTaskCount;

    std::unique_ptr<Image> zero_copy_image(ImageHelper<ImageWriteOnly<Image1dDefaults>>::create(context));

    ASSERT_TRUE(zero_copy_image->isMemObjZeroCopy());
    pCmdQ->taskCount = 40u;

    auto ptr = clEnqueueMapImage(
        pCmdQ,
        zero_copy_image.get(),
        CL_FALSE,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        &eventReturned,
        &retVal);

    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, eventReturned);

    EXPECT_EQ(ptr, zero_copy_image->getCpuAddressForMemoryTransfer());

    auto eventObject = castToObject<Event>(eventReturned);
    EXPECT_EQ(pCmdQ->taskCount, eventObject->peekTaskCount());
    EXPECT_TRUE(eventObject->updateStatusAndCheckCompletion());

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        zero_copy_image.get(),
        ptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(eventReturned);
}

TEST_F(EnqueueMapImageTest, GivenNonZeroCopyImageWhenMappedWithOffsetThenCorrectPointerIsReturned) {
    auto mapFlags = CL_MAP_WRITE;
    const size_t origin[3] = {1, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;

    Image *nonZeroCopyImage = ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context);

    EXPECT_FALSE(nonZeroCopyImage->isMemObjZeroCopy());

    auto ptr = clEnqueueMapImage(
        pCmdQ,
        nonZeroCopyImage,
        CL_TRUE,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        &retVal);

    float *HostPtrOffseted = (float *)Image1dDefaults::hostPtr + 1; //

    EXPECT_NE(nullptr, ptr);

    if (!image->isTiledAllocation()) {
        EXPECT_EQ(HostPtrOffseted, ptr); // Returned pointer should be offseted
    }

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        nonZeroCopyImage,
        ptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    delete nonZeroCopyImage;
}

HWTEST_F(EnqueueMapImageTest, givenSharingHandlerWhenNonReadOnlyMapAndUnmapOnNonTiledImageIsCalledThenMakeGpuCopy) {
    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context));
    ASSERT_NE(nullptr, image);
    image->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(image->isTiledAllocation());

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = 1;
    csr.taskLevel = 1;
    pCmdQ->taskCount = 1;
    pCmdQ->taskLevel = 1;

    size_t origin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};
    void *data = clEnqueueMapImage(pCmdQ, image.get(), CL_TRUE, CL_MAP_WRITE, origin, region, nullptr, nullptr, 0, NULL, NULL, &retVal);
    EXPECT_NE(nullptr, data);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, pCmdQ->taskCount);
    EXPECT_EQ(2u, pCmdQ->taskLevel);

    retVal = clEnqueueUnmapMemObject(pCmdQ, image.get(), data, 0, NULL, NULL);
    EXPECT_EQ(3u, pCmdQ->taskCount);
    EXPECT_EQ(3u, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueMapImageTest, givenSharingHandlerWhenReadOnlyMapAndUnmapOnNonTiledImageIsCalledThenMakeGpuCopy) {
    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context));
    ASSERT_NE(nullptr, image);
    image->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(image->isTiledAllocation());

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = 1;
    csr.taskLevel = 1;
    pCmdQ->taskCount = 1;
    pCmdQ->taskLevel = 1;

    size_t origin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};
    void *data = clEnqueueMapImage(pCmdQ, image.get(), CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, NULL, NULL, &retVal);
    EXPECT_NE(nullptr, data);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, pCmdQ->taskCount);
    EXPECT_EQ(2u, pCmdQ->taskLevel);

    retVal = clEnqueueUnmapMemObject(pCmdQ, image.get(), data, 0, NULL, NULL);
    EXPECT_EQ(2u, pCmdQ->taskCount);
    EXPECT_EQ(2u, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueMapImageTest, givenImageWithouUsetHostPtrFlagWhenMappedOnCpuThenSetAllMapProperties) {
    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context));
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    size_t origin[] = {2, 0, 0};
    size_t region[] = {2, 1, 1};
    void *mappedPtr = clEnqueueMapImage(pCmdQ, image.get(), CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, NULL, NULL, &retVal);
    EXPECT_NE(nullptr, mappedPtr);

    MapInfo mappedInfo;
    auto success = image->findMappedPtr(mappedPtr, mappedInfo);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, mappedInfo.ptr);

    EXPECT_EQ(origin[0], mappedInfo.offset[0]);
    EXPECT_EQ(origin[1], mappedInfo.offset[1]);
    EXPECT_EQ(origin[2], mappedInfo.offset[2]);

    EXPECT_EQ(region[0], mappedInfo.size[0]);
    EXPECT_EQ(region[1], mappedInfo.size[1]);
    EXPECT_EQ(region[2], mappedInfo.size[2]);

    auto expectedPtr = ptrOffset(image->getCpuAddressForMapping(), image->calculateOffsetForMapping(mappedInfo.offset));

    EXPECT_EQ(mappedPtr, expectedPtr);
}

HWTEST_F(EnqueueMapImageTest, givenImageWithUseHostPtrFlagWhenMappedOnCpuThenSetAllMapProperties) {
    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context));
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    size_t origin[] = {2, 0, 0};
    size_t region[] = {2, 1, 1};
    void *mappedPtr = clEnqueueMapImage(pCmdQ, image.get(), CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, NULL, NULL, &retVal);
    EXPECT_NE(nullptr, mappedPtr);

    MapInfo mappedInfo;
    auto success = image->findMappedPtr(mappedPtr, mappedInfo);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, mappedInfo.ptr);

    EXPECT_EQ(origin[0], mappedInfo.offset[0]);
    EXPECT_EQ(origin[1], mappedInfo.offset[1]);
    EXPECT_EQ(origin[2], mappedInfo.offset[2]);

    EXPECT_EQ(region[0], mappedInfo.size[0]);
    EXPECT_EQ(region[1], mappedInfo.size[1]);
    EXPECT_EQ(region[2], mappedInfo.size[2]);

    auto expectedPtr = ptrOffset(image->getCpuAddressForMapping(), image->calculateOffsetForMapping(mappedInfo.offset));

    EXPECT_EQ(mappedPtr, expectedPtr);
}

TEST_F(EnqueueMapImageTest, givenBlockedCommandQueueWhenBlockingMapWith2DImageIsEnqueuedAndEventAsynchrounouslyCompletedThenEnqueueFinishesWithoutStall) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;

    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
        }
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };
    MockEventWithSetCompleteOnUpdate blockingEvent(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 1);

    cl_event blockingClEvent = &blockingEvent;

    int32_t initialRefCountCmdQ = pCmdQ->getRefInternalCount();

    auto ptr = pCmdQ->enqueueMapImage(
        image,
        true,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        1,
        &blockingClEvent,
        nullptr,
        retVal);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(initialRefCountCmdQ, pCmdQ->getRefInternalCount());
}

TEST_F(EnqueueMapImageTest, givenBlockedCommandQueueWhenBlockingMapWith1DImageIsEnqueuedAndEventAsynchrounouslyCompletedThenEnqueueFinishesWithoutStall) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;

    Image *image1D = ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context);

    ASSERT_NE(nullptr, image1D);
    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
        }
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };
    MockEventWithSetCompleteOnUpdate blockingEvent(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 1);

    cl_event blockingClEvent = &blockingEvent;

    int32_t initialRefCountCmdQ = pCmdQ->getRefInternalCount();

    auto ptr = pCmdQ->enqueueMapImage(
        image1D,
        true,
        mapFlags,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        1,
        &blockingClEvent,
        nullptr,
        retVal);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(initialRefCountCmdQ, pCmdQ->getRefInternalCount());
    delete image1D;
}

TEST_F(EnqueueMapImageTest, givenBlockedCommandQueueWhenBlockingCpuMapIsCalledThenReturnRowPitchAndSlicePitch) {
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    struct MyMockUserEvent : public UserEvent {
        MyMockUserEvent() : UserEvent(nullptr) {}
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };

    std::unique_ptr<Image> image(ImageHelper<Image1dArrayDefaults>::create(context));
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MyMockUserEvent blockingEvent;
    cl_event blockingClEvent = &blockingEvent;

    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           1, &blockingClEvent, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, retImageRowPitch);
    EXPECT_NE(0u, retImageSlicePitch);

    image.reset(ImageHelper<Image1dDefaults>::create(context));
    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           1, &blockingClEvent, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, retImageRowPitch);
    EXPECT_EQ(0u, retImageSlicePitch);
}

TEST_F(EnqueueMapImageTest, givenZeroCopyImageWhenMappedOnCpuThenReturnImageRowAndSlicePitch) {
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    std::unique_ptr<Image> image(ImageHelper<Image1dArrayDefaults>::create(context));
    EXPECT_TRUE(image->mappingOnCpuAllowed());
    EXPECT_TRUE(image->isMemObjZeroCopy());

    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(image->getImageDesc().image_row_pitch, retImageRowPitch);
    EXPECT_EQ(image->getImageDesc().image_slice_pitch, retImageSlicePitch);
}

TEST_F(EnqueueMapImageTest, givenNonZeroCopyImageWhenMappedOnCpuThenReturnHostRowAndSlicePitch) {
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dArrayDefaults>>::create(context));
    EXPECT_TRUE(image->mappingOnCpuAllowed());
    EXPECT_FALSE(image->isMemObjZeroCopy());

    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(image->getHostPtrRowPitch(), retImageRowPitch);
    EXPECT_EQ(image->getHostPtrSlicePitch(), retImageSlicePitch);
}

TEST_F(EnqueueMapImageTest, givenZeroCopyImageWhenMappedOnGpuThenReturnHostRowAndSlicePitch) {
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    std::unique_ptr<Image> image(ImageHelper<Image1dArrayDefaults>::create(context));
    image->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(image->mappingOnCpuAllowed());
    EXPECT_TRUE(image->isMemObjZeroCopy());

    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(image->getHostPtrRowPitch(), retImageRowPitch);
    EXPECT_EQ(image->getHostPtrSlicePitch(), retImageSlicePitch);
}

TEST_F(EnqueueMapImageTest, givenNonZeroCopyImageWhenMappedOnGpuThenReturnHostRowAndSlicePitch) {
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dArrayDefaults>>::create(context));
    image->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(image->mappingOnCpuAllowed());
    EXPECT_FALSE(image->isMemObjZeroCopy());

    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(image->getHostPtrRowPitch(), retImageRowPitch);
    EXPECT_EQ(image->getHostPtrSlicePitch(), retImageSlicePitch);
}

TEST_F(EnqueueMapImageTest, givenMipMapImageWhenMappedThenReturnHostRowAndSlicePitch) {
    const size_t origin[4] = {0, 0, 0, 1};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    cl_image_desc imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.num_mip_levels = 10;
    imageDesc.image_width = 4;
    imageDesc.image_height = 4;
    imageDesc.image_depth = 4;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(context, &imageDesc));
    image->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    pCmdQ->enqueueMapImage(image.get(), true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(image->getHostPtrRowPitch(), retImageRowPitch);
    EXPECT_EQ(image->getHostPtrSlicePitch(), retImageSlicePitch);
}

TEST_F(EnqueueMapImageTest, givenImage1DArrayWhenEnqueueMapImageIsCalledThenReturnRowAndSlicePitchAreEqual) {
    class MockImage : public Image {
      public:
        MockImage(Context *context, cl_mem_flags flags, GraphicsAllocation *allocation, const SurfaceFormatInfo &surfaceFormat,
                  const cl_image_format &imageFormat, const cl_image_desc &imageDesc) : Image(context, flags,
                                                                                              0, nullptr,
                                                                                              imageFormat, imageDesc,
                                                                                              true,
                                                                                              allocation,
                                                                                              false, 0, 0,
                                                                                              surfaceFormat, nullptr) {
        }

        void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel) override {}
        void setMediaImageArg(void *memory) override {}
        void setMediaSurfaceRotation(void *memory) override {}
        void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) override {}
        void transformImage2dArrayTo3d(void *memory) override {}
        void transformImage3dTo2dArray(void *memory) override {}
    };

    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t retImageRowPitch = 0;
    size_t retImageSlicePitch = 0;

    cl_mem_flags flags = CL_MEM_READ_ONLY;

    cl_image_desc imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_width = 329;
    imageDesc.image_array_size = 48;
    imageDesc.image_row_pitch = 2688;
    imageDesc.image_slice_pitch = 10752;
    imageDesc.num_mip_levels = 0;

    size_t imgSize = imageDesc.image_slice_pitch * imageDesc.image_array_size;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_order = CL_RGBA;
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT16;

    const SurfaceFormatInfo *surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto allocation = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{imgSize});
    ASSERT_NE(allocation, nullptr);

    MockImage image(context, flags, allocation, *surfaceFormat, imageFormat, imageDesc);

    EXPECT_TRUE(image.mappingOnCpuAllowed());
    EXPECT_TRUE(image.isMemObjZeroCopy());

    pCmdQ->enqueueMapImage(&image, true, CL_MAP_READ, origin, region,
                           &retImageRowPitch, &retImageSlicePitch,
                           0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(retImageRowPitch, retImageSlicePitch);
}

struct EnqueueMapImageTypeTest : public CommandEnqueueFixture,
                                 public ::testing::Test {

    typedef CommandQueueHwFixture CommandQueueFixture;
    using CommandQueueHwFixture::pCmdQ;

    EnqueueMapImageTypeTest(void) {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        image = ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(&context);
    }

    void TearDown() override {
        delete image;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueMapImage(cl_bool blocking = CL_TRUE) {
        typedef ImageUseHostPtr<Image2dDefaults> Traits;

        size_t imageRowPitch;
        size_t imageSlicePitch;
        size_t origin[3] = {0, 0, 0};
        size_t region[3] = {Traits::imageDesc.image_width, Traits::imageDesc.image_height, Traits::imageDesc.image_depth};

        cl_int retVal = 0;
        auto mappedPtr = pCmdQ->enqueueMapImage(
            image,
            blocking,
            Traits::flags,
            origin,
            region,
            &imageRowPitch,
            &imageSlicePitch,
            0,
            nullptr,
            nullptr,
            retVal);
        EXPECT_NE(nullptr, mappedPtr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);
    }
    MockContext context;
    Image *image = nullptr;
};

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueMapImageTypeTest, blockingEnqueueRequiresPCWithDCFlushSetAfterWalker) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    // Set taskCount to 1 to call finish on map operation
    pCmdQ->taskCount = 1;
    bool blocking = true;
    enqueueMapImage<FamilyType>(blocking);

    auto itorWalker = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    auto *cmd = (PIPE_CONTROL *)*itorCmd;
    EXPECT_NE(cmdList.end(), itorCmd);

    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        // SKL: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_FALSE(cmd->getDcFlushEnable());
        // Move to next PPC
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmd = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_TRUE(cmd->getDcFlushEnable());
    } else {
        // single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

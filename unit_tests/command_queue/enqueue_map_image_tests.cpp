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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

struct EnqueueMapImageTest : public DeviceFixture,
                             public BuiltInFixture,
                             public CommandQueueHwFixture,
                             public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueMapImageTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        BuiltInFixture::SetUp(pDevice);
        CommandQueueFixture::SetUp(pDevice, 0);
        context = new MockContext(pDevice);
        image = ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context);
    }

    void TearDown() override {
        delete image;
        BuiltInFixture::TearDown();
        context->release();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    MockContext *context;
    cl_int retVal = CL_INVALID_VALUE;
    Image *image = nullptr;
    char srcMemory[128];
};

TEST_F(EnqueueMapImageTest, reuseMappedPtrForTiledImg) {
    if (!image->allowTiling()) {
        return;
    }
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {0, 0, 0};

    auto ptr1 = pCmdQ->enqueueMapImage(
        image, true, mapFlags, origin,
        region, nullptr, nullptr, 0,
        nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto ptr2 = pCmdQ->enqueueMapImage(
        image, true, mapFlags, origin,
        region, nullptr, nullptr, 0,
        nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(ptr1, ptr2);

    retVal = pCmdQ->enqueueUnmapMemObject(image, ptr1, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

template <typename GfxFamily>
struct mockedImage : public ImageHw<GfxFamily> {
    using ImageHw<GfxFamily>::ImageHw;
    void setAllocatedMappedPtr(void *allocatedMappedPtr) override {
        ownershipTaken = this->hasOwnership();
        MemObj::setAllocatedMappedPtr(allocatedMappedPtr);
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
                                      true,
                                      0,
                                      &surfaceFormatInfo,
                                      nullptr);

    mockImage.createFunction = image->createFunction;

    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {0, 0, 0};

    pCmdQ->enqueueMapImage(
        &mockImage, true, mapFlags, origin,
        region, nullptr, nullptr, 0,
        nullptr, nullptr, retVal);
    EXPECT_TRUE(mockImage.ownershipTaken);
}

TEST_F(EnqueueMapImageTest, checkPointer) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {0, 0, 0};
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
    const size_t region[3] = {0, 0, 0};
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

TEST_F(EnqueueMapImageTest, MapImageWaitEvent) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    cl_event eventReturned = nullptr;
    uint32_t tagHW = 0;
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {0, 0, 0};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    size_t GWS = 1;

    MockKernelWithInternals kernel(*pDevice);
    *pTagMemory = tagHW;
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
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
        &eventReturned,
        retVal);

    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(3u, taskCount);

    clSetEventCallback(eventReturned, CL_COMPLETE, E2Clb::SignalEv2, (void *)pTagMemory);

    retVal = clWaitForEvents(1, &eventReturned);
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
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(4u, taskCount);

    clReleaseEvent(eventReturned);
}

HWTEST_F(EnqueueMapImageTest, MapImageEventProperties) {
    cl_event eventReturned = nullptr;
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {0, 0, 0};
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
    const size_t region[3] = {1, 0, 0};
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
    //this is CPU path , event is manually set to completed state so task count equalizies to CSR = 100
    EXPECT_EQ(100u, eventObject->peekTaskCount());
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
    const size_t region[3] = {0, 0, 0};
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

    if (!image->allowTiling()) {
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

HWTEST_F(EnqueueMapImageTest, givenSharingHandlerWhenMapAndUnmapOnNonTiledImageIsCalledThenMakeGpuCopy) {
    auto image = ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context);
    ASSERT_NE(nullptr, image);
    image->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(image->allowTiling());

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = 1;
    csr.taskLevel = 1;
    pCmdQ->taskCount = 1;
    pCmdQ->taskLevel = 1;

    size_t origin[] = {0, 0, 0};
    size_t region[] = {1, 0, 0};
    void *data = clEnqueueMapImage(pCmdQ, image, CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, NULL, NULL, &retVal);
    EXPECT_NE(nullptr, data);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, pCmdQ->taskCount);
    EXPECT_EQ(2u, pCmdQ->taskLevel);

    retVal = clEnqueueUnmapMemObject(pCmdQ, image, data, 0, NULL, NULL);
    EXPECT_EQ(3u, pCmdQ->taskCount);
    EXPECT_EQ(3u, pCmdQ->taskLevel);

    delete image;
}

TEST_F(EnqueueMapImageTest, givenBlockedCommandQueueWhenBlockingMapWith2DImageIsEnqueuedAndEventAsynchrounouslyCompletedThenEnqueueFinishesWithoutStall) {
    auto mapFlags = CL_MAP_READ;
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {0, 0, 0};
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
    const size_t region[3] = {0, 0, 0};
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

struct EnqueueMapImageTypeTest : public CommandEnqueueFixture,
                                 public MemoryManagementFixture,
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

HWTEST_F(EnqueueMapImageTypeTest, blockingEnqueueRequiresPCWithDCFlushSetAfterWalker) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    // Set taskCount to 1 to call finish on map operation
    pCmdQ->taskCount = 1;
    bool blocking = true;
    enqueueMapImage<FamilyType>(blocking);

    auto itorWalker = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    auto *cmd = (PIPE_CONTROL *)*itorCmd;
    EXPECT_NE(cmdList.end(), itorCmd);

    if (::renderCoreFamily != IGFX_GEN8_CORE) {
        // SKL+: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
        // Move to next PPC
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmd = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_FALSE(cmd->getDcFlushEnable());
    } else {
        // BDW: single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/event/user_event.h"
#include "runtime/mem_obj/image.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_event.h"

using namespace NEO;

template <typename Family>
class MyMockCommandQueue : public CommandQueueHw<Family> {

  public:
    MyMockCommandQueue(Context *context, Device *device) : CommandQueueHw<Family>(context, device, nullptr){};

    cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite,
                             const size_t *origin, const size_t *region,
                             size_t inputRowPitch, size_t inputSlicePitch,
                             const void *ptr, GraphicsAllocation *mapAllocation,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override {
        passedBlockingWrite = blockingWrite;
        passedPtr = (void *)ptr;
        enqueueWriteImageCalled++;
        return CL_SUCCESS;
    }
    cl_int finish(bool dcFlush) override {
        EXPECT_TRUE(dcFlush);
        finishCalled++;
        return CL_SUCCESS;
    }
    void *passedPtr = nullptr;
    cl_bool passedBlockingWrite = CL_INVALID_VALUE;
    unsigned int enqueueWriteImageCalled = 0;
    unsigned int finishCalled = 0;
};

class ImageUnmapTest : public ::testing::Test {
  public:
    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        context = std::make_unique<MockContext>(device.get());
        image.reset(ImageHelper<ImageReadOnly<Image3dDefaults>>::create(context.get()));
    }
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Image> image;
};

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledThenEnqueueNonBlockingMapImage) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(context.get(), device.get()));
    void *ptr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    image->setAllocatedMapPtr(ptr);
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);

    if (UnitTestHelper<FamilyType>::tiledImagesSupported) {
        EXPECT_EQ(ptr, commandQueue->passedPtr);
        EXPECT_EQ((cl_bool)CL_FALSE, commandQueue->passedBlockingWrite);
        EXPECT_EQ(1u, commandQueue->enqueueWriteImageCalled);
    } else {
        EXPECT_EQ(0u, commandQueue->enqueueWriteImageCalled);
    }
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithMemUseHostPtrAndWithoutEventsThenFinishIsCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(context.get(), device.get()));
    image.reset(ImageHelper<ImageUseHostPtr<Image3dDefaults>>::create(context.get()));
    auto ptr = image->getBasePtrForMap();
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
    EXPECT_EQ(UnitTestHelper<FamilyType>::tiledImagesSupported ? 1u : 0u,
              commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithoutMemUseHostPtrThenFinishIsCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(context.get(), device.get()));
    auto ptr = image->getBasePtrForMap();
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 2, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
    EXPECT_EQ(UnitTestHelper<FamilyType>::tiledImagesSupported ? 1u : 0u,
              commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithMemUseHostPtrAndWithNotReadyEventsThenFinishIsNotCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(context.get(), device.get()));
    image.reset(ImageHelper<ImageUseHostPtr<Image3dDefaults>>::create(context.get()));

    MockEvent<UserEvent> mockEvent(context.get());
    mockEvent.setStatus(Event::eventNotReady);
    cl_event clEvent = &mockEvent;
    commandQueue->enqueueUnmapMemObject(image.get(), nullptr, 1, &clEvent, nullptr);
    EXPECT_EQ(0u, commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithMemUseHostPtrAndWithoutNotReadyEventsThenFinishIsCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(context.get(), device.get()));
    image.reset(ImageHelper<ImageUseHostPtr<Image3dDefaults>>::create(context.get()));
    MockEvent<UserEvent> mockEvent(context.get());
    mockEvent.setStatus(0);
    cl_event clEvent = &mockEvent;
    auto ptr = image->getBasePtrForMap();
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 1, &clEvent, nullptr);
    EXPECT_EQ(UnitTestHelper<FamilyType>::tiledImagesSupported ? 1u : 0u,
              commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenEnqueueMapImageIsCalledTwiceThenAllocatedMemoryPtrIsNotOverridden) {
    if (!UnitTestHelper<FamilyType>::tiledImagesSupported) {
        GTEST_SKIP();
    }
    cl_int retVal;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<CommandQueue> commandQueue(CommandQueue::create(context.get(), device.get(), nullptr, retVal));
    commandQueue->enqueueMapImage(image.get(), CL_FALSE, 0, origin, region, nullptr, nullptr, 0, nullptr, nullptr, retVal);
    EXPECT_NE(nullptr, image->getAllocatedMapPtr());
    void *ptr = image->getAllocatedMapPtr();
    EXPECT_EQ(alignUp(ptr, MemoryConstants::pageSize), ptr);
    commandQueue->enqueueMapImage(image.get(), CL_FALSE, 0, origin, region, nullptr, nullptr, 0, nullptr, nullptr, retVal);
    EXPECT_EQ(ptr, image->getAllocatedMapPtr());
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
}

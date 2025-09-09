/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {
class ClDevice;
class Context;
class GraphicsAllocation;
} // namespace NEO

using namespace NEO;

template <typename Family>
class MyMockCommandQueue : public CommandQueueHw<Family> {

  public:
    MyMockCommandQueue(Context *context, ClDevice *device) : CommandQueueHw<Family>(context, device, nullptr, false){};

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
    cl_int finish(bool resolvePendingL3Flushes) override {
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
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context = std::make_unique<MockContext>(device.get());
        image.reset(ImageHelperUlt<ImageReadOnly<Image3dDefaults>>::create(context.get()));
    }
    std::unique_ptr<MockClDevice> device;
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
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0, nullptr);

    AllocationProperties properties{0, false, MemoryConstants::cacheLineSize, AllocationType::mapAllocation, false, device->getDeviceBitfield()};
    auto allocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties, ptr);
    image->setMapAllocation(allocation);

    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);

    if (defaultHwInfo->capabilityTable.supportsImages) {
        EXPECT_EQ(ptr, commandQueue->passedPtr);
        EXPECT_EQ((cl_bool)CL_FALSE, commandQueue->passedBlockingWrite);
        EXPECT_EQ(1u, commandQueue->enqueueWriteImageCalled);
    } else {
        EXPECT_EQ(0u, commandQueue->enqueueWriteImageCalled);
    }
}

HWTEST_F(ImageUnmapTest, givenImageWhenEnqueueMapImageIsCalledTwiceThenAllocatedMemoryPtrIsNotOverridden) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    cl_int retVal;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};
    std::unique_ptr<CommandQueue> commandQueue(CommandQueue::create(context.get(), device.get(), nullptr, false, retVal));
    commandQueue->enqueueMapImage(image.get(), CL_FALSE, 0, origin, region, nullptr, nullptr, 0, nullptr, nullptr, retVal);
    EXPECT_NE(nullptr, image->getAllocatedMapPtr());
    void *ptr = image->getAllocatedMapPtr();
    EXPECT_EQ(alignUp(ptr, MemoryConstants::pageSize), ptr);
    commandQueue->enqueueMapImage(image.get(), CL_FALSE, 0, origin, region, nullptr, nullptr, 0, nullptr, nullptr, retVal);
    EXPECT_EQ(ptr, image->getAllocatedMapPtr());
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
}

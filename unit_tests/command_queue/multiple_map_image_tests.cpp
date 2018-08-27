/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

struct MultipleMapImageTest : public DeviceFixture, public ::testing::Test {
    template <typename T>
    struct MockImage : public ImageHw<T> {
        using Image::mapOperationsHandler;

        template <class... Params>
        MockImage(Params... params) : ImageHw<T>(params...) {
            this->createFunction = ImageHw<T>::create;
        };

        void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override {
            copyRegion = copySize;
            copyOrigin = copyOffset;
            transferToHostPtrCalled++;
        };
        void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override {
            copyRegion = copySize;
            copyOrigin = copyOffset;
            transferFromHostPtrCalled++;
        };

        MemObjSizeArray copyRegion = {{0, 0, 0}};
        MemObjOffsetArray copyOrigin = {{0, 0, 0}};
        uint32_t transferToHostPtrCalled = 0;
        uint32_t transferFromHostPtrCalled = 0;
    };

    template <typename T>
    struct MockCmdQ : public CommandQueueHw<T> {
        MockCmdQ(Context *context, Device *device) : CommandQueueHw<T>(context, device, 0) {}

        cl_int enqueueReadImage(Image *srcImage, cl_bool blockingRead, const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch, void *ptr,
                                cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
            enqueueRegion = {{region[0], region[1], region[2]}};
            enqueueOrigin = {{origin[0], origin[1], origin[2]}};
            readImageCalled++;
            if (failOnReadImage) {
                return CL_OUT_OF_RESOURCES;
            }
            return CommandQueueHw<T>::enqueueReadImage(srcImage, blockingRead, origin, region, rowPitch, slicePitch, ptr, numEventsInWaitList, eventWaitList, event);
        }

        cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite, const size_t *origin, const size_t *region, size_t inputRowPitch,
                                 size_t inputSlicePitch, const void *ptr, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                 cl_event *event) override {
            enqueueRegion = {{region[0], region[1], region[2]}};
            enqueueOrigin = {{origin[0], origin[1], origin[2]}};
            unmapPtr = ptr;
            writeImageCalled++;
            if (failOnWriteImage) {
                return CL_OUT_OF_RESOURCES;
            }
            return CommandQueueHw<T>::enqueueWriteImage(dstImage, blockingWrite, origin, region, inputRowPitch, inputSlicePitch, ptr,
                                                        numEventsInWaitList, eventWaitList, event);
        }

        cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
            enqueueMarkerCalled++;
            return CommandQueueHw<T>::enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);
        }

        uint32_t writeImageCalled = 0;
        uint32_t readImageCalled = 0;
        uint32_t enqueueMarkerCalled = 0;
        bool failOnReadImage = false;
        bool failOnWriteImage = false;
        MemObjSizeArray enqueueRegion = {{0, 0, 0}};
        MemObjOffsetArray enqueueOrigin = {{0, 0, 0}};
        const void *unmapPtr = nullptr;
    };

    template <typename Traits, typename FamilyType>
    std::unique_ptr<MockImage<FamilyType>> createMockImage() {
        auto mockAlloc = pDevice->getMemoryManager()->allocateGraphicsMemory(1024);
        auto tiledImage = GmmHelper::allowTiling(Traits::imageDesc);

        auto surfaceFormat = Image::getSurfaceFormatFromTable(Traits::flags, &Traits::imageFormat);
        auto img = new MockImage<FamilyType>(context, Traits::flags, 1024, Traits::hostPtr,
                                             Traits::imageFormat, Traits::imageDesc, false, mockAlloc, false,
                                             tiledImage, 0, 0, *surfaceFormat);
        return std::unique_ptr<MockImage<FamilyType>>(img);
    }

    template <typename FamilyType>
    std::unique_ptr<MockCmdQ<FamilyType>> createMockCmdQ() {
        return std::unique_ptr<MockCmdQ<FamilyType>>(new MockCmdQ<FamilyType>(context, pDevice));
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        context = new MockContext(pDevice);
    }

    void TearDown() override {
        delete context;
        DeviceFixture::TearDown();
    }

    MockContext *context = nullptr;
    cl_int retVal = CL_INVALID_VALUE;
};

HWTEST_F(MultipleMapImageTest, givenValidReadAndWriteImageWhenMappedOnGpuThenAddMappedPtrAndRemoveOnUnmap) {
    auto image = createMockImage<Image3dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{1, 2, 1}};
    MemObjSizeArray region = {{3, 4, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_WRITE, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->enqueueRegion, region);
    EXPECT_EQ(cmdQ->enqueueOrigin, origin);
    EXPECT_EQ(cmdQ->readImageCalled, 1u);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->enqueueRegion, region);
    EXPECT_EQ(cmdQ->enqueueOrigin, origin);
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtr);
    EXPECT_EQ(cmdQ->writeImageCalled, 1u);
}

HWTEST_F(MultipleMapImageTest, givenReadOnlyMapWhenUnmappedOnGpuThenEnqueueMarker) {
    auto image = createMockImage<Image3dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{1, 2, 1}};
    MemObjSizeArray region = {{3, 4, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_READ, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->enqueueRegion, region);
    EXPECT_EQ(cmdQ->enqueueOrigin, origin);
    EXPECT_EQ(cmdQ->readImageCalled, 1u);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->writeImageCalled, 0u);
    EXPECT_EQ(cmdQ->enqueueMarkerCalled, 1u);
}

HWTEST_F(MultipleMapImageTest, givenNotMappedPtrWhenUnmapedOnGpuThenReturnError) {
    auto image = createMockImage<Image2dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), image->getBasePtrForMap(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(MultipleMapImageTest, givenErrorFromReadImageWhenMappedOnGpuThenDontAddMappedPtr) {
    auto image = createMockImage<Image3dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(image->mappingOnCpuAllowed());
    cmdQ->failOnReadImage = true;

    size_t origin[] = {2, 1, 1};
    size_t region[] = {2, 1, 1};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(nullptr, mappedPtr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
}

HWTEST_F(MultipleMapImageTest, givenErrorFromWriteImageWhenUnmappedOnGpuThenDontRemoveMappedPtr) {
    auto image = createMockImage<Image3dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(image->mappingOnCpuAllowed());
    cmdQ->failOnWriteImage = true;

    size_t origin[] = {2, 1, 1};
    size_t region[] = {2, 1, 1};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_WRITE, origin, region, nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(cmdQ->writeImageCalled, 1u);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
}

HWTEST_F(MultipleMapImageTest, givenUnblockedQueueWhenMappedOnCpuThenAddMappedPtrAndRemoveOnUnmap) {
    auto image = createMockImage<Image1dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{1, 0, 0}};
    MemObjSizeArray region = {{3, 1, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_WRITE, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(1u, image->transferToHostPtrCalled);
    EXPECT_EQ(image->copyRegion, region);
    EXPECT_EQ(image->copyOrigin, origin);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(1u, image->transferFromHostPtrCalled);
    EXPECT_EQ(image->copyRegion, region);
    EXPECT_EQ(image->copyOrigin, origin);
}

HWTEST_F(MultipleMapImageTest, givenUnblockedQueueWhenReadOnlyMappedOnCpuThenDontMakeCpuCopy) {
    auto image = createMockImage<Image1dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{1, 0, 0}};
    MemObjSizeArray region = {{3, 1, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_READ, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(1u, image->transferToHostPtrCalled);
    EXPECT_EQ(image->copyRegion, region);
    EXPECT_EQ(image->copyOrigin, origin);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(0u, image->transferFromHostPtrCalled);
}

HWTEST_F(MultipleMapImageTest, givenBlockedQueueWhenMappedOnCpuThenAddMappedPtrAndRemoveOnUnmap) {
    auto image = createMockImage<Image1dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    UserEvent mapEvent, unmapEvent;
    cl_event clMapEvent = &mapEvent;
    cl_event clUnmapEvent = &unmapEvent;

    MemObjOffsetArray origin = {{1, 0, 0}};
    MemObjSizeArray region = {{3, 1, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_FALSE, CL_MAP_WRITE, &origin[0], &region[0], nullptr, nullptr, 1, &clMapEvent, nullptr, &retVal);
    mapEvent.setStatus(CL_COMPLETE);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->transferToHostPtrCalled);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(image->copyRegion, region);
    EXPECT_EQ(image->copyOrigin, origin);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 1, &clUnmapEvent, nullptr);
    unmapEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(1u, image->transferFromHostPtrCalled);
    EXPECT_EQ(image->copyRegion, region);
    EXPECT_EQ(image->copyOrigin, origin);
}

HWTEST_F(MultipleMapImageTest, givenBlockedQueueWhenMappedReadOnlyOnCpuThenDontMakeCpuCopy) {
    auto image = createMockImage<Image1dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    UserEvent mapEvent, unmapEvent;
    cl_event clMapEvent = &mapEvent;
    cl_event clUnmapEvent = &unmapEvent;

    MemObjOffsetArray origin = {{1, 0, 0}};
    MemObjSizeArray region = {{3, 1, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_FALSE, CL_MAP_READ, &origin[0], &region[0], nullptr, nullptr, 1, &clMapEvent, nullptr, &retVal);
    mapEvent.setStatus(CL_COMPLETE);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, image->transferToHostPtrCalled);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(image->copyRegion, region);
    EXPECT_EQ(image->copyOrigin, origin);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtr, 1, &clUnmapEvent, nullptr);
    unmapEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(0u, image->transferFromHostPtrCalled);
}

HWTEST_F(MultipleMapImageTest, givenInvalidPtrWhenUnmappedOnCpuThenReturnError) {
    auto image = createMockImage<Image1dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), image.get(), image->getBasePtrForMap(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(MultipleMapImageTest, givenMultimpleMapsWhenUnmappingThenRemoveCorrectPointers) {
    auto image = createMockImage<Image3dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();

    MapInfo mappedPtrs[3] = {
        {nullptr, 1, {{1, 1, 1}}, {{1, 1, 1}}, 0},
        {nullptr, 1, {{4, 4, 2}}, {{4, 4, 4}}, 0},
        {nullptr, 1, {{10, 10, 10}}, {{10, 10, 10}}, 0}};

    for (size_t i = 0; i < 3; i++) {
        mappedPtrs[i].ptr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_WRITE, &mappedPtrs[i].offset[0], &mappedPtrs[i].size[0],
                                              nullptr, nullptr, 0, nullptr, nullptr, &retVal);
        EXPECT_NE(nullptr, mappedPtrs[i].ptr);
        EXPECT_EQ(i + 1, image->mapOperationsHandler.size());
        EXPECT_EQ(cmdQ->enqueueRegion, mappedPtrs[i].size);
        EXPECT_EQ(cmdQ->enqueueOrigin, mappedPtrs[i].offset);
    }

    // reordered unmap
    clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtrs[1].ptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtrs[1].ptr);
    EXPECT_EQ(cmdQ->enqueueRegion, mappedPtrs[1].size);
    EXPECT_EQ(cmdQ->enqueueOrigin, mappedPtrs[1].offset);

    clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtrs[2].ptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtrs[2].ptr);
    EXPECT_EQ(cmdQ->enqueueRegion, mappedPtrs[2].size);
    EXPECT_EQ(cmdQ->enqueueOrigin, mappedPtrs[2].offset);

    clEnqueueUnmapMemObject(cmdQ.get(), image.get(), mappedPtrs[0].ptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, image->mapOperationsHandler.size());
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtrs[0].ptr);
    EXPECT_EQ(cmdQ->enqueueRegion, mappedPtrs[0].size);
    EXPECT_EQ(cmdQ->enqueueOrigin, mappedPtrs[0].offset);
}

HWTEST_F(MultipleMapImageTest, givenOverlapingPtrWhenMappingOnGpuForWriteThenReturnError) {
    auto image = createMockImage<Image3dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{1, 2, 1}};
    MemObjSizeArray region = {{3, 4, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_READ, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());

    origin[0]++;
    void *mappedPtr2 = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_WRITE, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(nullptr, mappedPtr2);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
}

HWTEST_F(MultipleMapImageTest, givenOverlapingPtrWhenMappingOnCpuForWriteThenReturnError) {
    auto image = createMockImage<Image1dDefaults, FamilyType>();
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{1, 0, 0}};
    MemObjSizeArray region = {{3, 1, 1}};
    void *mappedPtr = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_READ, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());

    origin[0]++;
    void *mappedPtr2 = clEnqueueMapImage(cmdQ.get(), image.get(), CL_TRUE, CL_MAP_WRITE, &origin[0], &region[0], nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(nullptr, mappedPtr2);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(1u, image->mapOperationsHandler.size());
}

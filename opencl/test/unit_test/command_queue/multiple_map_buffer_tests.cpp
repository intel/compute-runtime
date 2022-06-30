/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct MultipleMapBufferTest : public ClDeviceFixture, public ::testing::Test {
    template <typename T>
    struct MockBuffer : public BufferHw<T> {
        template <class... Params>
        MockBuffer(Params... params) : BufferHw<T>(std::forward<Params>(params)...) {
            this->createFunction = BufferHw<T>::create;
        };

        void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override {
            this->copySize = copySize[0];
            this->copyOffset = copyOffset[0];
            transferToHostPtrCalled++;
        };
        void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override {
            this->copySize = copySize[0];
            this->copyOffset = copyOffset[0];
            transferFromHostPtrCalled++;
        };

        size_t copySize = 0;
        size_t copyOffset = 0;
        uint32_t transferToHostPtrCalled = 0;
        uint32_t transferFromHostPtrCalled = 0;
    };

    template <typename T>
    struct MockCmdQ : public CommandQueueHw<T> {
        MockCmdQ(Context *context, ClDevice *device) : CommandQueueHw<T>(context, device, 0, false) {}

        cl_int enqueueReadBuffer(Buffer *buffer, cl_bool blockingRead, size_t offset, size_t size, void *ptr,
                                 GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                 cl_event *event) override {
            enqueueSize = size;
            enqueueOffset = offset;
            readBufferCalled++;
            if (failOnReadBuffer) {
                return CL_OUT_OF_RESOURCES;
            }
            return CommandQueueHw<T>::enqueueReadBuffer(buffer, blockingRead, offset, size, ptr, mapAllocation,
                                                        numEventsInWaitList, eventWaitList, event);
        }

        cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t cb, const void *ptr,
                                  GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                  cl_event *event) override {
            enqueueSize = cb;
            enqueueOffset = offset;
            unmapPtr = ptr;
            writeBufferCalled++;
            if (failOnWriteBuffer) {
                return CL_OUT_OF_RESOURCES;
            }
            return CommandQueueHw<T>::enqueueWriteBuffer(buffer, blockingWrite, offset, cb, ptr, mapAllocation,
                                                         numEventsInWaitList, eventWaitList, event);
        }

        cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
            enqueueMarkerCalled++;
            return CommandQueueHw<T>::enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);
        }

        uint32_t writeBufferCalled = 0;
        uint32_t readBufferCalled = 0;
        uint32_t enqueueMarkerCalled = 0;
        bool failOnReadBuffer = false;
        bool failOnWriteBuffer = false;
        size_t enqueueSize = 0;
        size_t enqueueOffset = 0;
        const void *unmapPtr = nullptr;
    };

    template <typename FamilyType>
    std::unique_ptr<MockBuffer<FamilyType>> createMockBuffer(bool mapOnGpu) {
        MemoryProperties memoryProperties;
        auto mockAlloc = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
        auto multiGraphicsAllocation = GraphicsAllocationHelper::toMultiGraphicsAllocation(mockAlloc);

        auto buffer = new MockBuffer<FamilyType>(context, memoryProperties, 0, 0, 1024, mockAlloc->getUnderlyingBuffer(), mockAlloc->getUnderlyingBuffer(),
                                                 std::move(multiGraphicsAllocation), false, false, false);
        if (mapOnGpu) {
            buffer->setSharingHandler(new SharingHandler());
            auto gfxAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());
            for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
                gfxAllocation->setGmm(new MockGmm(pDevice->getGmmHelper()), handleId);
            }
        }
        return std::unique_ptr<MockBuffer<FamilyType>>(buffer);
    }

    template <typename FamilyType>
    std::unique_ptr<MockCmdQ<FamilyType>> createMockCmdQ() {
        return std::unique_ptr<MockCmdQ<FamilyType>>(new MockCmdQ<FamilyType>(context, pClDevice));
    }

    void SetUp() override {
        ClDeviceFixture::SetUp();
        context = new MockContext(pClDevice);
    }

    void TearDown() override {
        delete context;
        ClDeviceFixture::TearDown();
    }

    MockContext *context = nullptr;
    cl_int retVal = CL_INVALID_VALUE;
};

HWTEST_F(MultipleMapBufferTest, givenValidReadAndWriteBufferWhenMappedOnGpuThenAddMappedPtrAndRemoveOnUnmap) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE, offset, size, 0, nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->readBufferCalled, 1u);
    EXPECT_EQ(cmdQ->enqueueSize, size);
    EXPECT_EQ(cmdQ->enqueueOffset, offset);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->writeBufferCalled, 1u);
    EXPECT_EQ(cmdQ->enqueueSize, size);
    EXPECT_EQ(cmdQ->enqueueOffset, offset);
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtr);
}

HWTEST_F(MultipleMapBufferTest, givenReadOnlyMapWhenUnmappedOnGpuThenEnqueueMarker) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_READ, offset, size, 0, nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->readBufferCalled, 1u);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->writeBufferCalled, 0u);
    EXPECT_EQ(cmdQ->enqueueMarkerCalled, 1u);
}

HWTEST_F(MultipleMapBufferTest, givenWriteInvalidateMapWhenMappedOnGpuThenCallEnqueueMarker) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION, offset, size, 0, nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->readBufferCalled, 0u);
    EXPECT_EQ(cmdQ->enqueueMarkerCalled, 1u);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->writeBufferCalled, 1u);
    EXPECT_EQ(cmdQ->enqueueMarkerCalled, 1u);
}

HWTEST_F(MultipleMapBufferTest, givenNotMappedPtrWhenUnmapedOnGpuThenReturnError) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());

    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), buffer->getBasePtrForMap(cmdQ->getDevice().getRootDeviceIndex()), 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(MultipleMapBufferTest, givenErrorFromReadBufferWhenMappedOnGpuThenDontAddMappedPtr) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    cmdQ->failOnReadBuffer = true;

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_READ, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(nullptr, mappedPtr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
}

HWTEST_F(MultipleMapBufferTest, givenErrorFromWriteBufferWhenUnmappedOnGpuThenDontRemoveMappedPtr) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    cmdQ->failOnWriteBuffer = true;

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, cmdQ->writeBufferCalled);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
}

HWTEST_F(MultipleMapBufferTest, givenUnblockedQueueWhenMappedOnCpuThenAddMappedPtrAndRemoveOnUnmap) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(1u, buffer->transferToHostPtrCalled);
    EXPECT_EQ(buffer->copySize, size);
    EXPECT_EQ(buffer->copyOffset, offset);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(1u, buffer->transferFromHostPtrCalled);
    EXPECT_EQ(buffer->copySize, size);
    EXPECT_EQ(buffer->copyOffset, offset);
}

HWTEST_F(MultipleMapBufferTest, givenUnblockedQueueWhenReadOnlyMappedOnCpuThenDontMakeCpuCopy) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_READ, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(1u, buffer->transferToHostPtrCalled);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(0u, buffer->transferFromHostPtrCalled);
}

HWTEST_F(MultipleMapBufferTest, givenUnblockedQueueWhenWriteInvalidateMappedOnCpuThenDontMakeCpuCopy) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(0u, buffer->transferToHostPtrCalled);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(1u, buffer->transferFromHostPtrCalled);
}

HWTEST_F(MultipleMapBufferTest, givenBlockedQueueWhenMappedOnCpuThenAddMappedPtrAndRemoveOnUnmap) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    UserEvent mapEvent, unmapEvent;
    cl_event clMapEvent = &mapEvent;
    cl_event clUnmapEvent = &unmapEvent;

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE, offset, size, 1, &clMapEvent, nullptr, &retVal);
    mapEvent.setStatus(CL_COMPLETE);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(buffer->copySize, size);
    EXPECT_EQ(buffer->copyOffset, offset);
    EXPECT_EQ(1u, buffer->transferToHostPtrCalled);

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 1, &clUnmapEvent, nullptr);
    unmapEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(buffer->copySize, size);
    EXPECT_EQ(buffer->copyOffset, offset);
    EXPECT_EQ(1u, buffer->transferFromHostPtrCalled);
}

HWTEST_F(MultipleMapBufferTest, givenBlockedQueueWhenMappedReadOnlyOnCpuThenDontMakeCpuCopy) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    UserEvent mapEvent, unmapEvent;
    cl_event clMapEvent = &mapEvent;
    cl_event clUnmapEvent = &unmapEvent;

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_READ, offset, size, 1, &clMapEvent, nullptr, &retVal);
    mapEvent.setStatus(CL_COMPLETE);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(1u, buffer->transferToHostPtrCalled);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtr, 1, &clUnmapEvent, nullptr);
    unmapEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(0u, buffer->transferFromHostPtrCalled);
}

HWTEST_F(MultipleMapBufferTest, givenInvalidPtrWhenUnmappedOnCpuThenReturnError) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    retVal = clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), buffer->getBasePtrForMap(cmdQ->getDevice().getRootDeviceIndex()), 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(MultipleMapBufferTest, givenMultimpleMapsWhenUnmappingThenRemoveCorrectPointers) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();

    MapInfo mappedPtrs[3] = {
        {nullptr, 1, {{1, 0, 0}}, {{1, 0, 0}}, 0},
        {nullptr, 1, {{2, 0, 0}}, {{2, 0, 0}}, 0},
        {nullptr, 1, {{5, 0, 0}}, {{5, 0, 0}}, 0},
    };

    for (size_t i = 0; i < 3; i++) {
        mappedPtrs[i].ptr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE,
                                               mappedPtrs[i].offset[0], mappedPtrs[i].size[0], 0, nullptr, nullptr, &retVal);
        EXPECT_NE(nullptr, mappedPtrs[i].ptr);
        EXPECT_EQ(i + 1, buffer->getMapOperationsHandler().size());
        EXPECT_EQ(cmdQ->enqueueSize, mappedPtrs[i].size[0]);
        EXPECT_EQ(cmdQ->enqueueOffset, mappedPtrs[i].offset[0]);
    }

    // reordered unmap
    clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtrs[1].ptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtrs[1].ptr);
    EXPECT_EQ(cmdQ->enqueueSize, mappedPtrs[1].size[0]);
    EXPECT_EQ(cmdQ->enqueueOffset, mappedPtrs[1].offset[0]);

    clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtrs[2].ptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtrs[2].ptr);
    EXPECT_EQ(cmdQ->enqueueSize, mappedPtrs[2].size[0]);
    EXPECT_EQ(cmdQ->enqueueOffset, mappedPtrs[2].offset[0]);

    clEnqueueUnmapMemObject(cmdQ.get(), buffer.get(), mappedPtrs[0].ptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, buffer->getMapOperationsHandler().size());
    EXPECT_EQ(cmdQ->unmapPtr, mappedPtrs[0].ptr);
    EXPECT_EQ(cmdQ->enqueueSize, mappedPtrs[0].size[0]);
    EXPECT_EQ(cmdQ->enqueueOffset, mappedPtrs[0].offset[0]);
}

HWTEST_F(MultipleMapBufferTest, givenOverlapingPtrWhenMappingOnGpuForWriteThenReturnError) {
    auto buffer = createMockBuffer<FamilyType>(true);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_READ, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());

    offset++;
    void *mappedPtr2 = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(nullptr, mappedPtr2);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
}

HWTEST_F(MultipleMapBufferTest, givenOverlapingPtrWhenMappingOnCpuForWriteThenReturnError) {
    auto buffer = createMockBuffer<FamilyType>(false);
    auto cmdQ = createMockCmdQ<FamilyType>();
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    size_t offset = 1;
    size_t size = 3;
    void *mappedPtr = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_READ, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());

    offset++;
    void *mappedPtr2 = clEnqueueMapBuffer(cmdQ.get(), buffer.get(), CL_FALSE, CL_MAP_WRITE, offset, size, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(nullptr, mappedPtr2);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(1u, buffer->getMapOperationsHandler().size());
}

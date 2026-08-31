/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"

#include <limits>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct MockSharingHandler : public SharingHandler {
    using SharingHandler::acquireCount;

    int synchronizeHandler(UpdateData &updateData) override {
        synchronizeHandlerCalled++;
        if (overrideSynchronizeHandler) {
            return synchronizeHandlerResult;
        }
        return SharingHandler::synchronizeHandler(updateData);
    }

    int validateUpdateData(UpdateData &updateData) override {
        validateUpdateDataCalled++;
        rootDeviceIndexSeenByValidate = updateData.rootDeviceIndex;
        memObjectSeenByValidate = updateData.memObject;
        sharedHandleSeenByValidate = updateData.sharedHandle;
        return validateUpdateDataResult;
    }

    void synchronizeObject(UpdateData &updateData) override {
        synchronizeObjectCalled++;
        rootDeviceIndexSeenBySynchronize = updateData.rootDeviceIndex;
        memObjectSeenBySynchronize = updateData.memObject;
        updateData.synchronizationStatus = synchronizationStatusToSet;
    }

    void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override {
        resolveGraphicsAllocationChangeCalled++;
        sharedHandleSeenByResolve = currentSharedHandle;
        statusSeenByResolve = updateData->synchronizationStatus;
    }

    void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override {
        releaseResourceCalled++;
        memObjectSeenByRelease = memObject;
        rootDeviceIndexSeenByRelease = rootDeviceIndex;
    }

    uint32_t synchronizeHandlerCalled = 0u;
    uint32_t validateUpdateDataCalled = 0u;
    uint32_t synchronizeObjectCalled = 0u;
    uint32_t resolveGraphicsAllocationChangeCalled = 0u;
    uint32_t releaseResourceCalled = 0u;

    int validateUpdateDataResult = CL_SUCCESS;
    int synchronizeHandlerResult = CL_SUCCESS;
    bool overrideSynchronizeHandler = false;
    SynchronizeStatus synchronizationStatusToSet = SynchronizeStatus::ACQUIRE_SUCCESFUL;
    SynchronizeStatus statusSeenByResolve = SynchronizeStatus::SHARED_OBJECT_NOT_CHANGED;

    MemObj *memObjectSeenByValidate = nullptr;
    MemObj *memObjectSeenBySynchronize = nullptr;
    MemObj *memObjectSeenByRelease = nullptr;
    uint32_t rootDeviceIndexSeenByValidate = std::numeric_limits<uint32_t>::max();
    uint32_t rootDeviceIndexSeenBySynchronize = std::numeric_limits<uint32_t>::max();
    uint32_t rootDeviceIndexSeenByRelease = std::numeric_limits<uint32_t>::max();
    osHandle sharedHandleSeenByValidate = 0u;
    osHandle sharedHandleSeenByResolve = 0u;
};

struct SharingHandlerFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        capturingContext->getDriverHandleCallBase = true;
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        rootDeviceIndex = clDevice->getRootDeviceIndex();
        memoryManager = driverHandle->getMemoryManager();
    }

    void TearDown() override {
        for (auto buffer : buffers) {
            delete buffer;
        }
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createSharedBuffer(SharingHandler *handler, osHandle sharedHandle = 0u) {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, allocationSize});
        allocation->setSharedHandle(sharedHandle);
        auto buffer = Buffer::createSharedBuffer(leoContext.get(), CL_MEM_READ_WRITE, handler,
                                                 GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation));
        buffers.push_back(buffer);
        return buffer;
    }

    static constexpr size_t allocationSize = 2 * MemoryConstants::pageSize;

    ClDevice *clDevice = nullptr;
    uint32_t rootDeviceIndex = 0u;
    MemoryManager *memoryManager = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
    std::vector<Buffer *> buffers{};
};

TEST_F(SharingHandlerFixture, givenFreshHandlerWhenAcquiringThenSynchronizationPathIsRunAndCountIsIncremented) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(1u, handler->synchronizeHandlerCalled);
    EXPECT_EQ(1u, handler->validateUpdateDataCalled);
    EXPECT_EQ(1u, handler->synchronizeObjectCalled);
    EXPECT_EQ(1u, handler->resolveGraphicsAllocationChangeCalled);
    EXPECT_EQ(1u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenFreshHandlerWhenAcquiringThenUpdateDataCarriesMemObjectAndRootDeviceIndex) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);

    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(static_cast<MemObj *>(buffer), handler->memObjectSeenByValidate);
    EXPECT_EQ(static_cast<MemObj *>(buffer), handler->memObjectSeenBySynchronize);
    EXPECT_EQ(rootDeviceIndex, handler->rootDeviceIndexSeenByValidate);
    EXPECT_EQ(rootDeviceIndex, handler->rootDeviceIndexSeenBySynchronize);
}

TEST_F(SharingHandlerFixture, givenAllocationWithSharedHandleWhenAcquiringThenUpdateDataStartsFromThatHandle) {
    constexpr osHandle expectedSharedHandle = 0x1234u;
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler, expectedSharedHandle);

    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(expectedSharedHandle, handler->sharedHandleSeenByValidate);
    EXPECT_EQ(expectedSharedHandle, handler->sharedHandleSeenByResolve);
}

TEST_F(SharingHandlerFixture, givenSuccessfulAcquireWhenResolvingAllocationChangeThenItSeesTheAcquiredStatus) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);

    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, handler->statusSeenByResolve);
}

TEST_F(SharingHandlerFixture, givenAlreadyAcquiredHandlerWhenAcquiringAgainThenOnlyTheCountIsIncremented) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(1u, handler->synchronizeHandlerCalled);
    EXPECT_EQ(1u, handler->synchronizeObjectCalled);
    EXPECT_EQ(1u, handler->resolveGraphicsAllocationChangeCalled);
    EXPECT_EQ(2u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenValidationFailureWhenAcquiringThenObjectIsNotSynchronizedAndCountStaysZero) {
    auto handler = new MockSharingHandler();
    handler->validateUpdateDataResult = CL_INVALID_MEM_OBJECT;
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(1u, handler->validateUpdateDataCalled);
    EXPECT_EQ(0u, handler->synchronizeObjectCalled);
    EXPECT_EQ(1u, handler->resolveGraphicsAllocationChangeCalled);
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenValidationFailureWhenAcquiringAgainThenSynchronizationIsRetried) {
    auto handler = new MockSharingHandler();
    handler->validateUpdateDataResult = CL_INVALID_MEM_OBJECT;
    auto buffer = createSharedBuffer(handler);
    ASSERT_EQ(CL_INVALID_MEM_OBJECT, handler->acquire(buffer, rootDeviceIndex));

    handler->validateUpdateDataResult = CL_SUCCESS;
    EXPECT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(2u, handler->validateUpdateDataCalled);
    EXPECT_EQ(1u, handler->synchronizeObjectCalled);
    EXPECT_EQ(1u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenObjectNotChangedAfterSynchronizationWhenAcquiringThenReturnsOutOfResources) {
    auto handler = new MockSharingHandler();
    handler->synchronizationStatusToSet = SynchronizeStatus::SHARED_OBJECT_NOT_CHANGED;
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, handler->acquire(buffer, rootDeviceIndex));
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenObjectRequiringUpdateAfterSynchronizationWhenAcquiringThenReturnsOutOfResources) {
    auto handler = new MockSharingHandler();
    handler->synchronizationStatusToSet = SynchronizeStatus::SHARED_OBJECT_REQUIRES_UPDATE;
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, handler->acquire(buffer, rootDeviceIndex));
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenSynchronizationErrorWhenAcquiringThenReturnsOutOfResources) {
    auto handler = new MockSharingHandler();
    handler->synchronizationStatusToSet = SynchronizeStatus::SYNCHRONIZE_ERROR;
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, handler->acquire(buffer, rootDeviceIndex));
    EXPECT_EQ(1u, handler->resolveGraphicsAllocationChangeCalled);
    EXPECT_EQ(SynchronizeStatus::SYNCHRONIZE_ERROR, handler->statusSeenByResolve);
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenOverriddenSynchronizeHandlerWhenAcquiringThenDefaultSynchronizationIsBypassed) {
    auto handler = new MockSharingHandler();
    handler->overrideSynchronizeHandler = true;
    handler->synchronizeHandlerResult = CL_OUT_OF_HOST_MEMORY;
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(1u, handler->synchronizeHandlerCalled);
    EXPECT_EQ(0u, handler->validateUpdateDataCalled);
    EXPECT_EQ(0u, handler->synchronizeObjectCalled);
    EXPECT_EQ(1u, handler->resolveGraphicsAllocationChangeCalled);
}

TEST_F(SharingHandlerFixture, givenOverriddenSynchronizeHandlerReportingSuccessWhenAcquiringThenStatusIsStillValidated) {
    auto handler = new MockSharingHandler();
    handler->overrideSynchronizeHandler = true;
    handler->synchronizeHandlerResult = CL_SUCCESS;
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, handler->acquire(buffer, rootDeviceIndex));
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenAcquiredHandlerWhenReleasingThenResourceIsReleasedWithTheOwningObject) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    handler->release(buffer, rootDeviceIndex);

    EXPECT_EQ(1u, handler->releaseResourceCalled);
    EXPECT_EQ(static_cast<MemObj *>(buffer), handler->memObjectSeenByRelease);
    EXPECT_EQ(rootDeviceIndex, handler->rootDeviceIndexSeenByRelease);
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenTwiceAcquiredHandlerWhenReleasedOnceThenResourceIsNotReleased) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    handler->release(buffer, rootDeviceIndex);

    EXPECT_EQ(0u, handler->releaseResourceCalled);
    EXPECT_EQ(1u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenTwiceAcquiredHandlerWhenReleasedTwiceThenResourceIsReleasedOnce) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    handler->release(buffer, rootDeviceIndex);
    handler->release(buffer, rootDeviceIndex);

    EXPECT_EQ(1u, handler->releaseResourceCalled);
    EXPECT_EQ(0u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenReleasedHandlerWhenAcquiringAgainThenSynchronizationRunsOnceMore) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);
    ASSERT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));
    handler->release(buffer, rootDeviceIndex);

    EXPECT_EQ(CL_SUCCESS, handler->acquire(buffer, rootDeviceIndex));

    EXPECT_EQ(2u, handler->synchronizeObjectCalled);
    EXPECT_EQ(1u, handler->acquireCount);
}

TEST_F(SharingHandlerFixture, givenBaseHandlerWhenAcquiringThenDefaultSynchronizeObjectReportsAnError) {
    auto handler = new SharingHandler();
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, handler->acquire(buffer, rootDeviceIndex));
}

TEST_F(SharingHandlerFixture, givenBaseHandlerWhenQueryingMemObjectInfoThenNothingIsProvided) {
    auto handler = new SharingHandler();
    createSharedBuffer(handler);

    size_t paramValueSize = 123u;
    void *paramValue = reinterpret_cast<void *>(0xDEADu);
    handler->getMemObjectInfo(paramValueSize, paramValue);

    EXPECT_EQ(123u, paramValueSize);
    EXPECT_EQ(reinterpret_cast<void *>(0xDEADu), paramValue);
}

TEST_F(SharingHandlerFixture, givenSharedBufferWhenCreatedThenTheHandlerIsOwnedByTheBuffer) {
    auto handler = new MockSharingHandler();
    auto buffer = createSharedBuffer(handler);

    EXPECT_EQ(handler, buffer->peekSharingHandler());
    EXPECT_EQ(handler, buffer->getSharingHandler().get());
}

TEST_F(SharingHandlerFixture, givenSharedBufferWhenSharingHandlerIsSharedWithAnotherObjectThenBothPointToTheSameHandler) {
    auto handler = new MockSharingHandler();
    auto parent = createSharedBuffer(handler);
    auto child = createSharedBuffer(new MockSharingHandler());

    child->setParentSharingHandler(parent->getSharingHandler());

    EXPECT_EQ(handler, child->peekSharingHandler());
    EXPECT_EQ(parent->peekSharingHandler(), child->peekSharingHandler());
}

TEST_F(SharingHandlerFixture, givenUpdateDataWhenConstructedThenDefaultsAreNeutral) {
    UpdateData updateData{7u};

    EXPECT_EQ(7u, updateData.rootDeviceIndex);
    EXPECT_EQ(SynchronizeStatus::SHARED_OBJECT_NOT_CHANGED, updateData.synchronizationStatus);
    EXPECT_EQ(0u, updateData.sharedHandle);
    EXPECT_EQ(nullptr, updateData.memObject);
    EXPECT_EQ(nullptr, updateData.updateData);
}

} // namespace ult
} // namespace LEO
} // namespace NEO

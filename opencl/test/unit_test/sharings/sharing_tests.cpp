/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(sharingHandler, givenBasicSharingHandlerWhenSynchronizeObjectThenErrorIsReturned) {
    struct SH : SharingHandler {
        int synchronizeHandlerMock(UpdateData &updateData) { return synchronizeHandler(updateData); }

    } sharingHandler;

    const uint32_t rootDeviceIndex = 1u;
    UpdateData updateData{rootDeviceIndex};
    sharingHandler.synchronizeHandlerMock(updateData);
    EXPECT_EQ(SynchronizeStatus::SYNCHRONIZE_ERROR, updateData.synchronizationStatus);

    size_t paramSize = 0;
    void *paramValue = nullptr;
    // there is no default implementation. parameters should be unchanged.
    sharingHandler.getMemObjectInfo(paramSize, paramValue);
    EXPECT_EQ(paramSize, 0u);
    EXPECT_EQ(paramValue, nullptr);
}

TEST(sharingHandler, givenMemObjWhenAcquireIncrementCounterThenReleaseShouldDecrementIt) {
    char buffer[64];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));
    std::unique_ptr<MemObj> memObj(
        new MemObj(&context, CL_MEM_OBJECT_BUFFER,
                   ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice()),
                   CL_MEM_USE_HOST_PTR, 0, sizeof(buffer), buffer, buffer, GraphicsAllocationHelper::toMultiGraphicsAllocation(mockAllocation), true, false, false));

    struct MockSharingHandler : SharingHandler {
        using SharingHandler::acquireCount;

        void synchronizeObject(UpdateData &updateData) override {
            updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
        }
    } sharingHandler;

    EXPECT_EQ(0u, sharingHandler.acquireCount);
    sharingHandler.acquire(memObj.get(), mockAllocation->getRootDeviceIndex());
    EXPECT_EQ(1u, sharingHandler.acquireCount);
    sharingHandler.release(memObj.get(), mockAllocation->getRootDeviceIndex());
    EXPECT_EQ(0u, sharingHandler.acquireCount);
}

TEST(sharingHandler, givenMemObjWhenAcquireTwoTimesThenReleaseShouldBeCalledTwoTimesToReleaseObject) {
    char buffer[64];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));
    std::unique_ptr<MemObj> memObj(
        new MemObj(&context, CL_MEM_OBJECT_BUFFER,
                   ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice()),
                   CL_MEM_USE_HOST_PTR, 0, sizeof(buffer), buffer, buffer, GraphicsAllocationHelper::toMultiGraphicsAllocation(mockAllocation), true, false, false));

    struct MockSharingHandler : SharingHandler {
        using SharingHandler::acquireCount;
        void synchronizeObject(UpdateData &updateData) override {
            updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
        }
        void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override {
            releaseCount++;
        };
        int releaseCount = 0;
    } sharingHandler;

    EXPECT_EQ(0u, sharingHandler.acquireCount);
    sharingHandler.acquire(memObj.get(), mockAllocation->getRootDeviceIndex());
    EXPECT_EQ(1u, sharingHandler.acquireCount);
    sharingHandler.acquire(memObj.get(), mockAllocation->getRootDeviceIndex());
    EXPECT_EQ(2u, sharingHandler.acquireCount);
    sharingHandler.release(memObj.get(), mockAllocation->getRootDeviceIndex());
    EXPECT_EQ(1u, sharingHandler.acquireCount);
    EXPECT_EQ(0, sharingHandler.releaseCount);
    sharingHandler.release(memObj.get(), mockAllocation->getRootDeviceIndex());
    EXPECT_EQ(0u, sharingHandler.acquireCount);
    EXPECT_EQ(1, sharingHandler.releaseCount);
}

TEST(sharingHandler, givenSharingHandlerWhenValidateUpdateDataIsCalledWithNonNullInputThenAbortIsNotCalled) {
    class MockSharingHandler : SharingHandler {
      public:
        using SharingHandler::validateUpdateData;
    };
    MockSharingHandler sharingHandler;
    const uint32_t rootDeviceIndex = 1u;
    UpdateData updateData{rootDeviceIndex};
    sharingHandler.validateUpdateData(updateData);
}

TEST(sharingHandler, givenSharingHandlerWhenAcquiringThenReturnErrorCode) {
    SharingHandler sharingHandler;
    MockContext context;
    MockGraphicsAllocation *graphicsAllocation = new MockGraphicsAllocation(nullptr, 0);
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER,
                  ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice()),
                  CL_MEM_USE_HOST_PTR, 0, 1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(graphicsAllocation), true, false, false);

    auto result = sharingHandler.acquire(&memObj, graphicsAllocation->getRootDeviceIndex());
    EXPECT_NE(CL_SUCCESS, result);
}

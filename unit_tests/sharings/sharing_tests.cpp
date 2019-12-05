/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/sharings/sharing.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(sharingHandler, givenBasicSharingHandlerWhenSynchronizeObjectThenErrorIsReturned) {
    struct SH : SharingHandler {
        int synchronizeHandlerMock(UpdateData &updateData) { return synchronizeHandler(updateData); }

    } sharingHandler;

    UpdateData updateData;
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
    std::unique_ptr<MemObj> memObj(new MemObj(&context, CL_MEM_OBJECT_BUFFER, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR, 0, 0), CL_MEM_USE_HOST_PTR, 0,
                                              sizeof(buffer), buffer, buffer, mockAllocation, true, false, false));

    struct MockSharingHandler : SharingHandler {
        unsigned int acquire(MemObj *memObj) {
            SharingHandler::acquire(memObj);
            return acquireCount;
        }
        unsigned int release(MemObj *memObj) {
            SharingHandler::release(memObj);
            return acquireCount;
        }
        void synchronizeObject(UpdateData &updateData) override {
            updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
        }
    } sharingHandler;

    EXPECT_EQ(sharingHandler.acquire(memObj.get()), 1u);
    EXPECT_EQ(sharingHandler.release(memObj.get()), 0u);
}

TEST(sharingHandler, givenMemObjWhenAcquireTwoTimesThenReleaseShouldBeCalledTwoTimesToReleaseObject) {
    char buffer[64];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));
    std::unique_ptr<MemObj> memObj(new MemObj(&context, CL_MEM_OBJECT_BUFFER, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR, 0, 0), CL_MEM_USE_HOST_PTR, 0,
                                              sizeof(buffer), buffer, buffer, mockAllocation, true, false, false));

    struct MockSharingHandler : SharingHandler {
        MockSharingHandler() {
            releaseCount = 0;
        }
        unsigned int acquire(MemObj *memObj) {
            SharingHandler::acquire(memObj);
            return acquireCount;
        }
        unsigned int release(MemObj *memObj) {
            SharingHandler::release(memObj);
            return acquireCount;
        }
        void synchronizeObject(UpdateData &updateData) override {
            updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
        }
        void releaseResource(MemObj *memObject) override {
            releaseCount++;
        };
        int releaseCount;
    } sharingHandler;

    EXPECT_EQ(sharingHandler.acquire(memObj.get()), 1u);
    EXPECT_EQ(sharingHandler.acquire(memObj.get()), 2u);
    EXPECT_EQ(sharingHandler.release(memObj.get()), 1u);
    EXPECT_EQ(sharingHandler.release(memObj.get()), 0u);
    EXPECT_EQ(sharingHandler.releaseCount, 1);
}

TEST(sharingHandler, givenSharingHandlerWhenValidateUpdateDataIsCalledWithNonNullInputThenAbortIsNotCalled) {
    class MockSharingHandler : SharingHandler {
      public:
        using SharingHandler::validateUpdateData;
    };
    MockSharingHandler sharingHandler;
    UpdateData updateData;
    sharingHandler.validateUpdateData(updateData);
}

TEST(sharingHandler, givenSharingHandlerWhenAcquiringThenReturnErrorCode) {
    SharingHandler sharingHandler;
    MockContext context;
    MockGraphicsAllocation *graphicsAllocation = new MockGraphicsAllocation(nullptr, 0);
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR, 0, 0), CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, graphicsAllocation, true, false, false);

    auto result = sharingHandler.acquire(&memObj);
    EXPECT_NE(CL_SUCCESS, result);
}

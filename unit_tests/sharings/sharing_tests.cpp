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

#include "gtest/gtest.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/sharings/sharing.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace OCLRT;

TEST(sharingHandler, givenBasicSharingHandlerWhenSynchronizeObjectThenErrorIsReturned) {
    struct SH : SharingHandler {
        void synchronizeHandlerMock(UpdateData *updateData) { return synchronizeHandler(updateData); }

    } sharingHandler;

    UpdateData updateData;
    sharingHandler.synchronizeHandlerMock(&updateData);
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
    std::unique_ptr<MemObj> memObj(new MemObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
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
        void synchronizeObject(UpdateData *updateData) override {
            updateData->synchronizationStatus = ACQUIRE_SUCCESFUL;
        }
    } sharingHandler;

    EXPECT_EQ(sharingHandler.acquire(memObj.get()), 1u);
    EXPECT_EQ(sharingHandler.release(memObj.get()), 0u);
}

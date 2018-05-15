/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/os_interface/windows/deferrable_deletion_win.h"
#include "unit_tests/mocks/mock_wddm20.h"
#include "gtest/gtest.h"
using namespace OCLRT;

class MockDeferrableDeletion : public DeferrableDeletionImpl {
  public:
    MockDeferrableDeletion(Wddm *wddm, D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue,
                           D3DKMT_HANDLE resourceHandle) : DeferrableDeletionImpl(wddm, handles, allocationCount, lastFenceValue, resourceHandle){};
    Wddm *getWddm() { return wddm; }
    D3DKMT_HANDLE *getHandles() { return handles; }
    uint32_t getAllocationCount() { return allocationCount; }
    uint64_t getLastFenceValue() { return lastFenceValue; }
    D3DKMT_HANDLE getResourceHandle() { return resourceHandle; }
};

class DeferrableDeletionTest : public ::testing::Test {
  public:
    WddmMock wddm;
    D3DKMT_HANDLE handle = 0;
    uint32_t allocationCount = 1;
    uint64_t lastFenceValue = 0;
    D3DKMT_HANDLE resourceHandle = 0;
};

TEST_F(DeferrableDeletionTest, givenDeferrableDeletionWhenIsCreatedThenObjectMembersAreSetProperly) {
    MockDeferrableDeletion deletion(&wddm, &handle, allocationCount, lastFenceValue, resourceHandle);
    EXPECT_EQ(&wddm, deletion.getWddm());
    EXPECT_NE(nullptr, deletion.getHandles());
    EXPECT_EQ(handle, *deletion.getHandles());
    EXPECT_NE(&handle, deletion.getHandles());
    EXPECT_EQ(allocationCount, deletion.getAllocationCount());
    EXPECT_EQ(lastFenceValue, deletion.getLastFenceValue());
    EXPECT_EQ(resourceHandle, deletion.getResourceHandle());
}

TEST_F(DeferrableDeletionTest, givenDeferrableDeletionWhenApplyIsCalledThenDeletionIsApplied) {
    wddm.callBaseDestroyAllocations = false;
    std::unique_ptr<DeferrableDeletion> deletion(DeferrableDeletion::create((Wddm *)&wddm, &handle, allocationCount, lastFenceValue, resourceHandle));
    EXPECT_EQ(0, wddm.destroyAllocationResult.called);
    deletion->apply();
    EXPECT_EQ(1, wddm.destroyAllocationResult.called);
}

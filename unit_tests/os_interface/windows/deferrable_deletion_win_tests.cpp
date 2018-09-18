/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/deferrable_deletion_win.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "gtest/gtest.h"
#include <type_traits>

using namespace OCLRT;

TEST(DeferrableDeletionImpl, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<DeferrableDeletionImpl>::value);
    EXPECT_FALSE(std::is_copy_constructible<DeferrableDeletionImpl>::value);
}

TEST(DeferrableDeletionImpl, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<DeferrableDeletionImpl>::value);
    EXPECT_FALSE(std::is_copy_assignable<DeferrableDeletionImpl>::value);
}

class MockDeferrableDeletion : public DeferrableDeletionImpl {
  public:
    using DeferrableDeletionImpl::allocationCount;
    using DeferrableDeletionImpl::DeferrableDeletionImpl;
    using DeferrableDeletionImpl::handles;
    using DeferrableDeletionImpl::resourceHandle;
    using DeferrableDeletionImpl::wddm;
};

class DeferrableDeletionTest : public ::testing::Test {
  public:
    WddmMock wddm;
    D3DKMT_HANDLE handle = 0;
    uint32_t allocationCount = 1;
    D3DKMT_HANDLE resourceHandle = 0;
};

TEST_F(DeferrableDeletionTest, givenDeferrableDeletionWhenIsCreatedThenObjectMembersAreSetProperly) {
    MockDeferrableDeletion deletion(&wddm, &handle, allocationCount, resourceHandle);
    EXPECT_EQ(&wddm, deletion.wddm);
    EXPECT_NE(nullptr, deletion.handles);
    EXPECT_EQ(handle, *deletion.handles);
    EXPECT_NE(&handle, deletion.handles);
    EXPECT_EQ(allocationCount, deletion.allocationCount);
    EXPECT_EQ(resourceHandle, deletion.resourceHandle);
}

TEST_F(DeferrableDeletionTest, givenDeferrableDeletionWhenApplyIsCalledThenDeletionIsApplied) {
    wddm.callBaseDestroyAllocations = false;
    std::unique_ptr<DeferrableDeletion> deletion(DeferrableDeletion::create((Wddm *)&wddm, &handle, allocationCount, resourceHandle));
    EXPECT_EQ(0, wddm.destroyAllocationResult.called);
    deletion->apply();
    EXPECT_EQ(1, wddm.destroyAllocationResult.called);
}

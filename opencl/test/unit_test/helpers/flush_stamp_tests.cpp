/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flush_stamp.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(FlushStampTest, referenceTrackedFlushStamp) {
    FlushStampTracker *flushStampTracker = new FlushStampTracker(true);
    auto flushStampSharedHandle = flushStampTracker->getStampReference();
    ASSERT_NE(nullptr, flushStampSharedHandle);

    EXPECT_EQ(1, flushStampSharedHandle->getRefInternalCount());
    EXPECT_EQ(0, flushStampSharedHandle->getRefApiCount());

    flushStampSharedHandle->incRefInternal();
    EXPECT_EQ(2, flushStampSharedHandle->getRefInternalCount());

    delete flushStampTracker;
    EXPECT_EQ(1, flushStampSharedHandle->getRefInternalCount());

    flushStampSharedHandle->decRefInternal();
}

TEST(FlushStampTest, dontAllocateStamp) {
    FlushStampTracker flushStampTracker(false);
    EXPECT_EQ(nullptr, flushStampTracker.getStampReference());
}

TEST(FlushStampTest, updateStampValue) {
    FlushStampTracker flushStampTracker(true);

    FlushStamp flushStamp = 0;
    EXPECT_EQ(flushStamp, flushStampTracker.peekStamp());

    flushStamp = 2;
    flushStampTracker.setStamp(flushStamp);
    EXPECT_EQ(flushStamp, flushStampTracker.peekStamp());
}

TEST(FlushStampTest, handleStampObjReplacing) {
    FlushStampTracker flushStampTracker(true);
    EXPECT_EQ(1, flushStampTracker.getStampReference()->getRefInternalCount()); //obj to release

    auto stampObj = new FlushStampTrackingObj();
    EXPECT_EQ(0, stampObj->getRefInternalCount()); // no owner

    flushStampTracker.replaceStampObject(stampObj);
    EXPECT_EQ(stampObj, flushStampTracker.getStampReference());
    EXPECT_EQ(1, stampObj->getRefInternalCount());
}

TEST(FlushStampTest, ignoreNullptrReplace) {
    FlushStampTracker flushStampTracker(true);
    auto currentObj = flushStampTracker.getStampReference();

    flushStampTracker.replaceStampObject(nullptr);
    EXPECT_EQ(currentObj, flushStampTracker.getStampReference());
}

TEST(FlushStampUpdateHelperTest, manageRefCounts) {
    FlushStampTrackingObj obj1, obj2;
    {
        FlushStampUpdateHelper updater;

        obj1.incRefInternal();
        obj2.incRefInternal();

        EXPECT_EQ(1, obj1.getRefInternalCount());
        EXPECT_EQ(1, obj2.getRefInternalCount());

        updater.insert(&obj1);
        updater.insert(&obj2);

        EXPECT_EQ(1, obj1.getRefInternalCount());
        EXPECT_EQ(1, obj2.getRefInternalCount());
    }

    EXPECT_EQ(1, obj1.getRefInternalCount());
    EXPECT_EQ(1, obj2.getRefInternalCount());
}

TEST(FlushStampUpdateHelperTest, multipleInserts) {
    FlushStampTrackingObj obj1;
    {
        FlushStampUpdateHelper updater;

        obj1.incRefInternal();

        EXPECT_EQ(1, obj1.getRefInternalCount());

        updater.insert(&obj1);
        updater.insert(&obj1);
        EXPECT_EQ(2u, updater.size());

        obj1.incRefInternal();
        updater.insert(&obj1);
        updater.insert(&obj1);
        EXPECT_EQ(4u, updater.size());

        EXPECT_EQ(2, obj1.getRefInternalCount());
    }
    EXPECT_EQ(2, obj1.getRefInternalCount());
    obj1.decRefInternal();
}

TEST(FlushStampUpdateHelperTest, ignoreNullptr) {
    FlushStampUpdateHelper updater;
    updater.insert(nullptr);
    EXPECT_EQ(0u, updater.size());
}

TEST(FlushStampUpdateHelperTest, givenUninitializedFlushStampWhenUpdateAllIsCalledThenItIsUpdated) {
    FlushStampUpdateHelper updater;
    FlushStampTrackingObj obj1;
    updater.insert(&obj1);

    FlushStamp flushStampToUpdate = 2;

    updater.updateAll(flushStampToUpdate);

    EXPECT_EQ(flushStampToUpdate, obj1.flushStamp);
    EXPECT_TRUE(obj1.initialized);
}

TEST(FlushStampUpdateHelperTest, givenFlushStampWhenSetStampWithZeroIsCalledThenFlushStampIsNotInitialized) {
    FlushStamp zeroStamp = 0;
    FlushStampTracker tracker(true);
    tracker.setStamp(zeroStamp);
    EXPECT_FALSE(tracker.getStampReference()->initialized);
}

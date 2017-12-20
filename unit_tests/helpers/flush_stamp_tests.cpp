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

#include "runtime/helpers/flush_stamp.h"
#include "gtest/gtest.h"

using namespace OCLRT;

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

        EXPECT_EQ(2, obj1.getRefInternalCount());
        EXPECT_EQ(2, obj2.getRefInternalCount());
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

        EXPECT_EQ(3, obj1.getRefInternalCount());
    }
    EXPECT_EQ(1, obj1.getRefInternalCount());
}

TEST(FlushStampUpdateHelperTest, ignoreNullptr) {
    FlushStampUpdateHelper updater;
    updater.insert(nullptr);
    EXPECT_EQ(0u, updater.size());
}

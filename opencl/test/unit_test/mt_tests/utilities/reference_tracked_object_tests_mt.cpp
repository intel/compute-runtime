/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/reference_tracked_object.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>

namespace NEO {

struct MockReferenceTrackedObject : ReferenceTrackedObject<MockReferenceTrackedObject> {
    MockReferenceTrackedObject(std::atomic<int> &marker,
                               std::atomic<bool> &flagInsideCustomDeleter,
                               std::atomic<bool> &flagUseCustomDeleter,
                               std::atomic<bool> &flagAfterBgDecRefCount)
        : marker(marker), flagInsideCustomDeleter(flagInsideCustomDeleter),
          flagUseCustomDeleter(flagUseCustomDeleter), flagAfterBgDecRefCount(flagAfterBgDecRefCount) {
    }

    using DeleterFuncType = void (*)(MockReferenceTrackedObject *);
    DeleterFuncType getCustomDeleter() const {
        if (flagUseCustomDeleter == false) {
            return nullptr;
        }

        flagInsideCustomDeleter = true;
        while (flagAfterBgDecRefCount == false) {
        }
        const_cast<MockReferenceTrackedObject *>(this)->setMarker(marker);

        return nullptr;
    }

    virtual void setMarker(std::atomic<int> &marker) {
        marker = getMarker();
    }

    static int getMarker() {
        return 1;
    }

    std::atomic<int> &marker;
    std::atomic<bool> &flagInsideCustomDeleter;
    std::atomic<bool> &flagUseCustomDeleter;
    std::atomic<bool> &flagAfterBgDecRefCount;
};

struct MockReferenceTrackedObjectDerivative : MockReferenceTrackedObject {
    using MockReferenceTrackedObject::MockReferenceTrackedObject;

    void setMarker(std::atomic<int> &marker) override {
        marker = getMarker();
    }

    static int getMarker() {
        return 2;
    }
};

void decRefCount(MockReferenceTrackedObject *obj, bool useInternalRefCount, std::atomic<bool> *flagInsideCustomDeleter, std::atomic<bool> *flagUseCustomDeleter, std::atomic<bool> *flagAfterBgDecRefCount) {
    while (*flagInsideCustomDeleter == false) {
    }

    *flagUseCustomDeleter = false;
    if (useInternalRefCount) {
        obj->decRefInternal();
    } else {
        obj->decRefApi();
    }

    *flagAfterBgDecRefCount = true;
}

TEST(ReferenceTrackedObject, whenDecreasingApiRefcountSimultaneouslyThenRetrieveProperCustomDeleterWhileObjectIsStillAlive) {
    ASSERT_NE(MockReferenceTrackedObjectDerivative::getMarker(), MockReferenceTrackedObject::getMarker());

    std::atomic<int> marker;
    std::atomic<bool> flagInsideCustomDeleter;
    std::atomic<bool> flagUseCustomDeleter;
    std::atomic<bool> flagAfterBgDecRefCount;
    marker = 0;
    flagInsideCustomDeleter = false;
    flagUseCustomDeleter = true;
    flagAfterBgDecRefCount = false;

    MockReferenceTrackedObjectDerivative *obj =
        new MockReferenceTrackedObjectDerivative(marker, flagInsideCustomDeleter, flagUseCustomDeleter, flagAfterBgDecRefCount);
    obj->incRefApi();
    obj->incRefApi();
    ASSERT_EQ(2, obj->getRefApiCount());
    ASSERT_EQ(2, obj->getRefInternalCount());
    ASSERT_EQ(0, marker);

    std::thread bgThread(decRefCount, obj, false, &flagInsideCustomDeleter, &flagUseCustomDeleter, &flagAfterBgDecRefCount);
    obj->decRefApi();
    bgThread.join();

    EXPECT_EQ(MockReferenceTrackedObjectDerivative::getMarker(), marker);
}

TEST(ReferenceTrackedObject, whenDecreasingInternalRefcountSimultaneouslyThenRetrieveProperCustomDeleterWhileObjectIsStillAlive) {
    ASSERT_NE(MockReferenceTrackedObjectDerivative::getMarker(), MockReferenceTrackedObject::getMarker());

    std::atomic<int> marker;
    std::atomic<bool> flagInsideCustomDeleter;
    std::atomic<bool> flagUseCustomDeleter;
    std::atomic<bool> flagAfterBgDecRefCount;
    marker = 0;
    flagInsideCustomDeleter = false;
    flagUseCustomDeleter = true;
    flagAfterBgDecRefCount = false;

    MockReferenceTrackedObjectDerivative *obj =
        new MockReferenceTrackedObjectDerivative(marker, flagInsideCustomDeleter, flagUseCustomDeleter, flagAfterBgDecRefCount);
    obj->incRefInternal();
    obj->incRefInternal();
    ASSERT_EQ(2, obj->getRefInternalCount());
    ASSERT_EQ(0, obj->getRefApiCount());
    ASSERT_EQ(0, marker);

    std::thread bgThread(decRefCount, obj, true, &flagInsideCustomDeleter, &flagUseCustomDeleter, &flagAfterBgDecRefCount);
    obj->decRefInternal();
    bgThread.join();

    EXPECT_EQ(MockReferenceTrackedObjectDerivative::getMarker(), marker);
}
} // namespace NEO

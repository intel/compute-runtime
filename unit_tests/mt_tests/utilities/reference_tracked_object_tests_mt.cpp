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

#include "runtime/utilities/reference_tracked_object.h"

#include "gtest/gtest.h"
#include <atomic>
#include <thread>

namespace OCLRT {

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
        const_cast<MockReferenceTrackedObject *>(this)->SetMarker(marker);

        return nullptr;
    }

    virtual void SetMarker(std::atomic<int> &marker) {
        marker = GetMarker();
    }

    static int GetMarker() {
        return 1;
    }

    std::atomic<int> &marker;
    std::atomic<bool> &flagInsideCustomDeleter;
    std::atomic<bool> &flagUseCustomDeleter;
    std::atomic<bool> &flagAfterBgDecRefCount;
};

struct MockReferenceTrackedObjectDerivative : MockReferenceTrackedObject {
    using MockReferenceTrackedObject::MockReferenceTrackedObject;

    void SetMarker(std::atomic<int> &marker) override {
        marker = GetMarker();
    }

    static int GetMarker() {
        return 2;
    }
};

void DecRefCount(MockReferenceTrackedObject *obj, bool useInternalRefCount, std::atomic<bool> *flagInsideCustomDeleter, std::atomic<bool> *flagUseCustomDeleter, std::atomic<bool> *flagAfterBgDecRefCount) {
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

TEST(ReferenceTrackedObject, whenDecreasingApiRefcountSimultaneouslyWillRetrieveProperCustomDeleterWhileObjectIsStillAlive) {
    ASSERT_NE(MockReferenceTrackedObjectDerivative::GetMarker(), MockReferenceTrackedObject::GetMarker());

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

    std::thread bgThread(DecRefCount, obj, false, &flagInsideCustomDeleter, &flagUseCustomDeleter, &flagAfterBgDecRefCount);
    obj->decRefApi();
    bgThread.join();

    EXPECT_EQ(MockReferenceTrackedObjectDerivative::GetMarker(), marker);
}

TEST(ReferenceTrackedObject, whenDecreasingInternalRefcountSimultaneouslyWillRetrieveProperCustomDeleterWhileObjectIsStillAlive) {
    ASSERT_NE(MockReferenceTrackedObjectDerivative::GetMarker(), MockReferenceTrackedObject::GetMarker());

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

    std::thread bgThread(DecRefCount, obj, true, &flagInsideCustomDeleter, &flagUseCustomDeleter, &flagAfterBgDecRefCount);
    obj->decRefInternal();
    bgThread.join();

    EXPECT_EQ(MockReferenceTrackedObjectDerivative::GetMarker(), marker);
}
} // namespace OCLRT

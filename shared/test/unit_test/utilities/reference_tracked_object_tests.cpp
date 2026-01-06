/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/reference_tracked_object.h"

#include "gtest/gtest.h"

#include <thread>

namespace NEO {
TEST(RefCounter, WhenIncrementingAndDecrementingThenCounterIsCorrect) {
    RefCounter<> rc;
    ASSERT_EQ(0, rc.peek());
    int max = 7;
    for (int i = 0; i < max; ++i) {
        rc.inc();
    }
    ASSERT_EQ(max, rc.peek());
    for (int i = 0; i < max - 1; ++i) {
        rc.dec();
    }
    ASSERT_EQ(1, rc.peek());
    rc.dec();
    ASSERT_EQ(0, rc.peek());
}

TEST(RefCounter, givenReferenceTrackedObjectWhenDecAndReturnCurrentIsCalledThenMinusOneIsReturned) {
    RefCounter<> rc;
    EXPECT_EQ(-1, rc.decAndReturnCurrent());
}

TEST(UniquePtrIfUnused, GivenDefaultWhenCreatedThenReturnNullptr) {
    unique_ptr_if_unused<int> uptr;
    ASSERT_EQ(nullptr, uptr.get());
    ASSERT_FALSE(uptr.isUnused());
}

TEST(UniquePtrIfUnused, WhenDeletingThenDeletionIsDeferred) {
    struct PrimitivObject {
        PrimitivObject(int v, bool *wasDeletedFlag)
            : memb(v), wasDeletedFlag(wasDeletedFlag) {
            if (wasDeletedFlag != nullptr) {
                *wasDeletedFlag = false;
            }
        }
        ~PrimitivObject() {
            if (wasDeletedFlag != nullptr) {
                *wasDeletedFlag = true;
            }
        }
        int memb;
        bool *wasDeletedFlag;
    };

    {
        bool wasDeleted = false;
        auto a = new PrimitivObject(5, &wasDeleted);
        {
            unique_ptr_if_unused<PrimitivObject> shouldNotDelete(a, false);
            EXPECT_FALSE(wasDeleted);
            EXPECT_FALSE(shouldNotDelete.isUnused());
            EXPECT_EQ(a, shouldNotDelete.get());
            EXPECT_EQ(a, &*shouldNotDelete);
            EXPECT_EQ(&a->memb, &shouldNotDelete->memb);
        }
        EXPECT_FALSE(wasDeleted);
        delete a;
        EXPECT_TRUE(wasDeleted);
    }

    {
        bool wasDeleted = false;
        auto a = new PrimitivObject(5, &wasDeleted);
        {
            unique_ptr_if_unused<PrimitivObject> shouldDelete(a, true);
            EXPECT_FALSE(wasDeleted);
            EXPECT_TRUE(shouldDelete.isUnused());
            EXPECT_EQ(a, shouldDelete.get());
            EXPECT_EQ(a, &*shouldDelete);
            EXPECT_EQ(&a->memb, &shouldDelete->memb);
        }
        EXPECT_TRUE(wasDeleted);
    }
}

TEST(UniquePtrIfUnused, GivenNoCustomDeleterAtCreationWhenDeletingThenUseDefaultDeleter) {
    bool deleterWasCalled{false};
    struct DefaultDeleterTestStruct {
        DefaultDeleterTestStruct(bool *deleterWasCalledFlag)
            : deleterWasCalledFlag(deleterWasCalledFlag) {}
        ~DefaultDeleterTestStruct() {
            *deleterWasCalledFlag = true;
        }
        bool *deleterWasCalledFlag;
    };

    {
        unique_ptr_if_unused<DefaultDeleterTestStruct> uptr(new DefaultDeleterTestStruct(&deleterWasCalled), true);
    }

    ASSERT_TRUE(deleterWasCalled);
}

TEST(UniquePtrIfUnused, GivenCustomDeleterAtCreationWhenDeletingThenUseProvidedDeleter) {
    struct CustomDeleterTestStruct {
        bool customDeleterWasCalled{false};
        static void deleter(CustomDeleterTestStruct *ptr) {
            ptr->customDeleterWasCalled = true;
        }
    } customDeleterObj;
    {
        unique_ptr_if_unused<CustomDeleterTestStruct> uptr(&customDeleterObj, true, &CustomDeleterTestStruct::deleter);
    }
    ASSERT_TRUE(customDeleterObj.customDeleterWasCalled);
}

TEST(UniquePtrIfUnused, GivenInitializedWithDerivativeOfReferenceCounterWhenDestroyingThenUseObtainedDeleter) {
    struct ObtainedDeleterTestStruct : public ReferenceTrackedObject<ObtainedDeleterTestStruct> {
        DeleterFuncType getCustomDeleter() const {
            return &ObtainedDeleterTestStruct::deleter;
        }

        bool obtainedDeleterWasCalled{false};
        static void deleter(ObtainedDeleterTestStruct *ptr) {
            ptr->obtainedDeleterWasCalled = true;
        }
    } obtainedDeleterObj;
    {
        unique_ptr_if_unused<ObtainedDeleterTestStruct> uptr(&obtainedDeleterObj, true);
    }
    ASSERT_TRUE(obtainedDeleterObj.obtainedDeleterWasCalled);

    {
        // make sure that nullptr for types that declare custom deleters don't cause problems
        unique_ptr_if_unused<ObtainedDeleterTestStruct>{nullptr, true};
        unique_ptr_if_unused<ObtainedDeleterTestStruct>{nullptr, false};
    }
}

TEST(ReferenceTrackedObject, GivenInternalAndApiReferenceCountWhenDecrementingThenDeletionBehavesCorrectly) {
    struct PrimitiveReferenceTrackedObject : ReferenceTrackedObject<PrimitiveReferenceTrackedObject> {
        PrimitiveReferenceTrackedObject(int v, bool *wasDeletedFlag)
            : memb(v), wasDeletedFlag(wasDeletedFlag) {
            if (wasDeletedFlag != nullptr) {
                *wasDeletedFlag = false;
            }
        }
        ~PrimitiveReferenceTrackedObject() override {
            if (wasDeletedFlag != nullptr) {
                *wasDeletedFlag = true;
            }
        }
        int memb;
        bool *wasDeletedFlag;
    };

    PrimitiveReferenceTrackedObject *a = nullptr;

    // 1. Test simple inc/dec api ref count
    {
        bool wasDeleted = false;
        a = new PrimitiveReferenceTrackedObject(5, &wasDeleted);
        ASSERT_EQ(0, a->getRefInternalCount());
        a->incRefApi();
        ASSERT_EQ(1, a->getRefInternalCount());
        {
            ASSERT_FALSE(wasDeleted);
            auto autoDeleter = a->decRefApi();
            EXPECT_TRUE(autoDeleter.isUnused());
            EXPECT_EQ(a, autoDeleter.get());
            EXPECT_EQ(a, &*autoDeleter);
            EXPECT_EQ(&a->memb, &autoDeleter->memb);
            ASSERT_FALSE(wasDeleted);
        }
        ASSERT_TRUE(wasDeleted);
    }

    // 2. Test zero api ref count, but internal ref count > 0
    {
        bool wasDeleted = false;
        a = new PrimitiveReferenceTrackedObject(5, &wasDeleted);
        ASSERT_EQ(0, a->getRefInternalCount());
        a->incRefInternal();
        ASSERT_EQ(1, a->getRefInternalCount());
        a->incRefApi();
        {
            {
                ASSERT_FALSE(wasDeleted);
                auto autoDeleter = a->decRefApi();
                EXPECT_FALSE(autoDeleter.isUnused());
                EXPECT_EQ(a, autoDeleter.get());
                EXPECT_EQ(a, &*autoDeleter);
                EXPECT_EQ(&a->memb, &autoDeleter->memb);
                ASSERT_FALSE(wasDeleted);
            }
            // 3. Test api ref count 0 and dec internal ref count to 0
            ASSERT_FALSE(wasDeleted);
            auto autoDeleter = a->decRefInternal(); // NOLINT(clang-analyzer-cplusplus.NewDelete)
            EXPECT_TRUE(autoDeleter.isUnused());
            EXPECT_EQ(a, autoDeleter.get());
            EXPECT_EQ(a, &*autoDeleter);
            EXPECT_EQ(&a->memb, &autoDeleter->memb);
            ASSERT_FALSE(wasDeleted);
        }
        ASSERT_TRUE(wasDeleted);
    }

    // 4. Test that api refcount is not affected by internal refcount
    bool wasDeleted = false;
    a = new PrimitiveReferenceTrackedObject(5, &wasDeleted);
    static const int refApiTop = 7;
    static const int refIntTop = 13;
    for (int i = 0; i < refApiTop; ++i) {
        a->incRefApi();
    }
    for (int i = 0; i < refIntTop; ++i) {
        a->incRefInternal();
    }

    EXPECT_EQ(refApiTop, a->getRefApiCount());
    EXPECT_EQ(refIntTop + refApiTop, a->getRefInternalCount());

    while (a->getRefApiCount() > 0) {
        a->decRefApi();
        ASSERT_FALSE(wasDeleted);
    }

    while (a->getRefInternalCount() > 1) {
        a->decRefInternal();
        ASSERT_FALSE(wasDeleted);
    }

    a->decRefInternal();
    ASSERT_TRUE(wasDeleted);
}

TEST(ReferenceTrackedObject, whenNewReferenceTrackedObjectIsCreatedThenRefcountsAreZero) {
    struct PrimitiveReferenceTrackedObject : ReferenceTrackedObject<PrimitiveReferenceTrackedObject> {
    };

    PrimitiveReferenceTrackedObject obj;
    EXPECT_EQ(0, obj.getRefApiCount());
    EXPECT_EQ(0, obj.getRefInternalCount());
}
} // namespace NEO

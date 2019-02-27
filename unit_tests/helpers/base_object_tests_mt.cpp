/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/platform/platform.h"

#include "gtest/gtest.h"

namespace OCLRT {

template <typename TypeParam>
struct BaseObjectTestsMt : public ::testing::Test {
    static void takeOwnerFailThreadFunc(TypeParam *obj) {
        auto ret = obj->takeOwnership(false);
        EXPECT_EQ(false, ret);
    }
    static void takeOwnerWaitThreadFunc(TypeParam *obj) {
        auto ret = obj->takeOwnership(true);
        EXPECT_EQ(true, ret);
        obj->releaseOwnership();
    }
};

typedef ::testing::Types<
    Platform,
    IntelAccelerator,
    CommandQueue>
    BaseObjectTypes;

TYPED_TEST_CASE(BaseObjectTestsMt, BaseObjectTypes);

// "typedef" BaseObjectTestsMt template to use with different TypeParams for testing
template <typename T>
using BaseObjectWithDefaultCtorTests = BaseObjectTestsMt<T>;

TYPED_TEST(BaseObjectTestsMt, takeOwner) {
    TypeParam *object = new TypeParam;
    bool ret = object->takeOwnership(false);
    EXPECT_EQ(true, ret);

    std::thread t1(BaseObjectTestsMt<TypeParam>::takeOwnerFailThreadFunc, object);
    t1.join();

    EXPECT_EQ(0U, object->getCond().peekNumWaiters());

    std::thread t2(BaseObjectTestsMt<TypeParam>::takeOwnerWaitThreadFunc, object);
    //wait on condition var counter, so current threads know t2 thread waits on this variable
    while (object->getCond().peekNumWaiters() == 0U) {
        std::this_thread::yield();
    }

    //t2 thread waits on conditional varialbe within takeOwnership
    EXPECT_EQ(1U, object->getCond().peekNumWaiters());
    std::this_thread::yield();

    //current thread releases ownership, so t2 can take it
    object->releaseOwnership();
    t2.join();

    object->release();
}
} // namespace OCLRT

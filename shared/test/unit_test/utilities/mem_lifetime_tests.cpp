/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/mem_lifetime.h"

#include "gtest/gtest.h"

struct OnlyFwdDeclaration;

struct ExtAsMember {
    int x = 6;
    int y = 7;
    Ext<OnlyFwdDeclaration> extMember;
};

struct TestStruct {
    bool wasCloned = false;
};

template <>
void cloneExt(ExtUniquePtrT<TestStruct> &dst, const TestStruct *src) {
    dst.reset(new TestStruct{true});
}

template <>
void destroyExt(TestStruct *dst) {
    delete dst;
}

TEST(Ext, GivenOnlyFwdDeclarationThenIsAbleToCompile) {
    // Check if copy-able even though OnlyFwdDeclaration is an incomplete type
    static_assert(std::is_copy_assignable<Ext<OnlyFwdDeclaration>>());
    static_assert(std::is_copy_constructible<Ext<OnlyFwdDeclaration>>());
    static_assert(std::is_copy_assignable<ExtAsMember>());
    static_assert(std::is_copy_constructible<ExtAsMember>());

    // Check if move-able even though OnlyFwdDeclaration is an incomplete type
    static_assert(std::is_move_assignable<Ext<OnlyFwdDeclaration>>());
    static_assert(std::is_move_constructible<Ext<OnlyFwdDeclaration>>());
    static_assert(std::is_move_assignable<ExtAsMember>());
    static_assert(std::is_move_constructible<ExtAsMember>());
}

TEST(Ext, GivenCloneFunctionThenUsesIt) {
    Ext<TestStruct> a{new TestStruct};
    EXPECT_FALSE(a.ptr->wasCloned);

    Ext<TestStruct> b = a;
    EXPECT_TRUE(b.ptr->wasCloned);

    Ext<TestStruct> c;
    c = a;
    EXPECT_TRUE(c.ptr->wasCloned);

    EXPECT_FALSE(a.ptr->wasCloned);
    auto &ra = a;
    a = ra;
    EXPECT_FALSE(a.ptr->wasCloned);
}

TEST(Clonable, GivenTypeThenProvidesClonableUniquePtr) {
    Clonable<int> a = {new int{5}};
    Clonable<int> b;
    b = a;
    Clonable<int> c{b};

    EXPECT_EQ(5, *a.ptr);
    EXPECT_EQ(5, *b.ptr);
    EXPECT_EQ(5, *c.ptr);

    EXPECT_NE(a.ptr.get(), b.ptr.get());
    EXPECT_NE(a.ptr.get(), c.ptr.get());
    EXPECT_NE(b.ptr.get(), c.ptr.get());

    auto &ra = a;
    a = ra;
    EXPECT_EQ(5, *a.ptr);

    Clonable<int> d;
    Clonable<int> e{d};
    EXPECT_EQ(nullptr, d.ptr.get());
    EXPECT_EQ(nullptr, e.ptr.get());
    d = e;
    EXPECT_EQ(nullptr, d.ptr.get());
    auto &re = e;
    e = re;
    EXPECT_EQ(nullptr, e.ptr.get());
}
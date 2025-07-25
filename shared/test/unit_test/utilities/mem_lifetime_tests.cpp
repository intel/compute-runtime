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
void cloneExt(ExtUniquePtrT<TestStruct> &dst, const TestStruct &src) {
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

struct PtrOpsMock {
    auto operator*() const noexcept {
        opCalled |= deref;
        return *this;
    }

    auto operator->() const noexcept {
        opCalled |= arrow;
        return this;
    }

    explicit operator bool() const noexcept {
        opCalled |= boolCast;
        return true;
    }

    enum OpCalled : uint32_t {
        deref = 1 << 0,
        arrow = 1 << 1,
        boolCast = 1 << 2,
        compareAsLhs = 1 << 3,
        compareAsRhs = 1 << 4,
        compare = 1 << 5,
    };

    mutable uint32_t opCalled = 0;
};

static bool operator==(const PtrOpsMock &lhs, const void *) {
    lhs.opCalled |= PtrOpsMock::compareAsLhs;
    return true;
}

static bool operator==(const void *, const PtrOpsMock &rhs) {
    rhs.opCalled |= PtrOpsMock::compareAsRhs;
    return true;
}

static bool operator==(const PtrOpsMock &lhs, const PtrOpsMock &rhs) {
    lhs.opCalled |= PtrOpsMock::compare;
    rhs.opCalled |= PtrOpsMock::compare;
    return true;
}

TEST(Clonable, WhenUsingUniquePtrWrapperOpsThenForwardsToParentsPtr) {
    struct Parent : Impl::UniquePtrWrapperOps<Parent> {
        PtrOpsMock ptr;
    };

    {
        Parent parent;
        [[maybe_unused]] auto res = *parent;
        EXPECT_EQ(PtrOpsMock::deref, parent.ptr.opCalled);
    }

    {
        Parent parent;
        [[maybe_unused]] auto res = parent->opCalled;
        EXPECT_EQ(PtrOpsMock::arrow, parent.ptr.opCalled);
    }

    {
        Parent parent;
        [[maybe_unused]] auto res = static_cast<bool>(parent);
        EXPECT_EQ(PtrOpsMock::boolCast, parent.ptr.opCalled);
    }

    {
        Parent parent;
        [[maybe_unused]] auto res = parent == nullptr;
        EXPECT_EQ(PtrOpsMock::compareAsLhs, parent.ptr.opCalled);
    }

    {
        Parent parent;
        [[maybe_unused]] auto res = nullptr == parent;
        EXPECT_EQ(PtrOpsMock::compareAsRhs, parent.ptr.opCalled);
    }

    {
        Parent parentA;
        Parent parentB;
        [[maybe_unused]] auto res = parentA == parentB;
        EXPECT_EQ(PtrOpsMock::compare, parentA.ptr.opCalled);
        EXPECT_EQ(PtrOpsMock::compare, parentB.ptr.opCalled);
    }
}

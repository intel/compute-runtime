/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gtest/gtest.h"
#include "test_mode.h"

template <typename Fixture>
struct Test
    : public Fixture,
      public ::testing::Test {

    void SetUp() override {
        Fixture::setUp();
    }

    void TearDown() override {
        Fixture::tearDown();
    }
};

#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)

#define TEST_MAX_LENGTH 140
#define CHECK_TEST_NAME_LENGTH_WITH_MAX(test_fixture, test_name, max_length) \
    static_assert((NEO::defaultTestMode != NEO::TestMode::AubTests && NEO::defaultTestMode != NEO::TestMode::AubTestsWithTbx) || (sizeof(#test_fixture) + sizeof(#test_name) <= max_length), "Test and fixture names length exceeds max allowed size: " TO_STR(max_length));
#define CHECK_TEST_NAME_LENGTH(test_fixture, test_name) \
    CHECK_TEST_NAME_LENGTH_WITH_MAX(test_fixture, test_name, TEST_MAX_LENGTH)

#ifdef TEST_F
#undef TEST_F
#endif

// Taken from gtest.h
#define TEST_F(test_fixture, test_name)                \
    CHECK_TEST_NAME_LENGTH(test_fixture, test_name)    \
    GTEST_TEST_(test_fixture, test_name, test_fixture, \
                ::testing::internal::GetTypeId<test_fixture>())

#ifdef TEST_P
#undef TEST_P
#endif

// Taken from gtest.h
#define TEST_P(test_suite_name, test_name)                                                                                                                \
    CHECK_TEST_NAME_LENGTH(test_fixture, test_name)                                                                                                       \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                              \
        : public test_suite_name {                                                                                                                        \
      public:                                                                                                                                             \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                                \
        () {}                                                                                                                                             \
        void TestBody() override;                                                                                                                         \
                                                                                                                                                          \
      private:                                                                                                                                            \
        static int AddToRegistry() {                                                                                                                      \
            ::testing::UnitTest::GetInstance()->parameterized_test_registry().GetTestCasePatternHolder<test_suite_name>(                                  \
                                                                                 #test_suite_name, ::testing::internal::CodeLocation(__FILE__, __LINE__)) \
                ->AddTestPattern(                                                                                                                         \
                    #test_suite_name,                                                                                                                     \
                    #test_name,                                                                                                                           \
                    new ::testing::internal::TestMetaFactory<                                                                                             \
                        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)>());                                                                           \
            return 0;                                                                                                                                     \
        }                                                                                                                                                 \
        static int gtest_registering_dummy_;                                                                                                              \
        GTEST_DISALLOW_COPY_AND_ASSIGN_(                                                                                                                  \
            GTEST_TEST_CLASS_NAME_(test_suite_name, test_name));                                                                                          \
    };                                                                                                                                                    \
    int GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                                           \
                               test_name)::gtest_registering_dummy_ =                                                                                     \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();                                                                              \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody()

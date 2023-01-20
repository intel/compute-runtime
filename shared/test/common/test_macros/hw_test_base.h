/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"
#include "shared/test/common/test_macros/test_excludes.h"

#define HWTEST_EXCLUDE_PRODUCT(test_suite_name, test_name, family)                  \
    struct test_suite_name##test_name##_PLATFORM_EXCLUDES_EXCLUDE_##family {        \
        test_suite_name##test_name##_PLATFORM_EXCLUDES_EXCLUDE_##family() {         \
            NEO::TestExcludes::addTestExclude(#test_suite_name #test_name, family); \
        }                                                                           \
    } test_suite_name##test_name##_PLATFORM_EXCLUDES_EXCLUDE_##family##_init;

#define IS_TEST_EXCLUDED(test_suite_name, test_name) \
    NEO::TestExcludes::isTestExcluded(#test_suite_name #test_name, ::productFamily, ::renderCoreFamily)

#define CALL_IF_MATCH(match_core, match_product, expr)                           \
    auto matchCore = match_core;                                                 \
    auto matchProduct = match_product;                                           \
    if ((::renderCoreFamily == matchCore) &&                                     \
        (IGFX_MAX_PRODUCT == matchProduct || ::productFamily == matchProduct)) { \
        expr;                                                                    \
    }

#define GENTEST_F(gfx_core, test_fixture, test_name)                 \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     gfx_core, IGFX_MAX_PRODUCT)

#define GENTEST_P(gfx_core, test_suite_name, test_name) \
    FAMILYTEST_TEST_P(test_suite_name, test_name, gfx_core, IGFX_MAX_PRODUCT)

#define FAMILYTEST_TEST_(test_suite_name, test_name, parent_class, parent_id, match_core, match_product) \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                   \
                                                                                                         \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                     \
      public:                                                                                            \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                               \
        () {}                                                                                            \
                                                                                                         \
      private:                                                                                           \
        template <typename FamilyType>                                                                   \
        void testBodyHw();                                                                               \
                                                                                                         \
        void TestBody() override {                                                                       \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                         \
                CALL_IF_MATCH(match_core, match_product,                                                 \
                              testBodyHw<typename NEO::GfxFamilyMapper<match_core>::GfxFamily>())        \
            }                                                                                            \
        }                                                                                                \
        void SetUp() override {                                                                          \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                          \
                GTEST_SKIP();                                                                            \
            }                                                                                            \
            CALL_IF_MATCH(match_core, match_product, parent_class::SetUp())                              \
        }                                                                                                \
        void TearDown() override {                                                                       \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                         \
                CALL_IF_MATCH(match_core, match_product, parent_class::TearDown())                       \
            }                                                                                            \
        }                                                                                                \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                            \
        GTEST_DISALLOW_COPY_AND_ASSIGN_(                                                                 \
            GTEST_TEST_CLASS_NAME_(test_suite_name, test_name));                                         \
    };                                                                                                   \
                                                                                                         \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =          \
        ::testing::internal::MakeAndRegisterTestInfo(                                                    \
            #test_suite_name, #test_name, nullptr, nullptr,                                              \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                          \
            ::testing::internal::SuiteApiResolver<                                                       \
                parent_class>::GetSetUpCaseOrSuite(),                                                    \
            ::testing::internal::SuiteApiResolver<                                                       \
                parent_class>::GetTearDownCaseOrSuite(),                                                 \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(                             \
                test_suite_name, test_name)>);                                                           \
    template <typename FamilyType>                                                                       \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::testBodyHw()

#define FAMILYTEST_TEST_P(test_suite_name, test_name, match_core, match_product)                                                                          \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                                    \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public test_suite_name {                                                                   \
      public:                                                                                                                                             \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                                \
        () {}                                                                                                                                             \
        template <typename FamilyType>                                                                                                                    \
        void testBodyHw();                                                                                                                                \
                                                                                                                                                          \
        void TestBody() override {                                                                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                          \
                CALL_IF_MATCH(match_core, match_product,                                                                                                  \
                              testBodyHw<typename NEO::GfxFamilyMapper<match_core>::GfxFamily>())                                                         \
            }                                                                                                                                             \
        }                                                                                                                                                 \
        void SetUp() override {                                                                                                                           \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                           \
                GTEST_SKIP();                                                                                                                             \
            }                                                                                                                                             \
            CALL_IF_MATCH(match_core, match_product, test_suite_name::SetUp())                                                                            \
        }                                                                                                                                                 \
        void TearDown() override {                                                                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                          \
                CALL_IF_MATCH(match_core, match_product, test_suite_name::TearDown())                                                                     \
            }                                                                                                                                             \
        }                                                                                                                                                 \
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
    template <typename FamilyType>                                                                                                                        \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::testBodyHw()

/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"
#include "shared/test/common/test_macros/test_excludes.h"

#ifdef NEO_IGNORE_INVALID_TEST_EXCLUDES
constexpr bool ignoreInvalidTestExcludes = true;
#else
constexpr bool ignoreInvalidTestExcludes = false;
#endif

#define TEST_EXCLUDE_VARIABLE(test_suite_name, test_name) Test_##test_suite_name##_##test_name##_NotFound_PleaseConsiderRemovingExclude

#define HWTEST_EXCLUDE_PRODUCT(test_suite_name, test_name, family)                  \
    extern bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                  \
    struct test_suite_name##test_name##_PLATFORM_EXCLUDES_EXCLUDE_##family {        \
        test_suite_name##test_name##_PLATFORM_EXCLUDES_EXCLUDE_##family() {         \
            NEO::TestExcludes::addTestExclude(#test_suite_name #test_name, family); \
            if constexpr (!ignoreInvalidTestExcludes) {                             \
                TEST_EXCLUDE_VARIABLE(test_suite_name, test_name) = true;           \
            }                                                                       \
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

#define FAMILYTEST_TEST_(test_suite_name, test_name, parent_class, parent_id, match_core, match_product)                                    \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                      \
                                                                                                                                            \
    bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                                                                                 \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                                                        \
      public:                                                                                                                               \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                  \
        () {}                                                                                                                               \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                  \
        (const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete;                                                              \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                  \
        (GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) = delete;                                                                   \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &operator=(const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete; \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &operator=(GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) = delete;      \
                                                                                                                                            \
      private:                                                                                                                              \
        template <typename FamilyType>                                                                                                      \
        void testBodyHw();                                                                                                                  \
                                                                                                                                            \
        void TestBody() override {                                                                                                          \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                CALL_IF_MATCH(match_core, match_product,                                                                                    \
                              testBodyHw<typename NEO::GfxFamilyMapper<match_core>::GfxFamily>())                                           \
            }                                                                                                                               \
        }                                                                                                                                   \
        void SetUp() override {                                                                                                             \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                             \
                GTEST_SKIP();                                                                                                               \
            }                                                                                                                               \
            CALL_IF_MATCH(match_core, match_product, parent_class::SetUp())                                                                 \
        }                                                                                                                                   \
        void TearDown() override {                                                                                                          \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                CALL_IF_MATCH(match_core, match_product, parent_class::TearDown())                                                          \
            }                                                                                                                               \
        }                                                                                                                                   \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                                                               \
    };                                                                                                                                      \
                                                                                                                                            \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =                                             \
        ::testing::internal::MakeAndRegisterTestInfo(                                                                                       \
            #test_suite_name, #test_name, nullptr, nullptr,                                                                                 \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                                                             \
            ::testing::internal::SuiteApiResolver<                                                                                          \
                parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__),                                                                     \
            ::testing::internal::SuiteApiResolver<                                                                                          \
                parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__),                                                                  \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(                                                                \
                test_suite_name, test_name)>);                                                                                              \
    template <typename FamilyType>                                                                                                          \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::testBodyHw()

#define FAMILYTEST_TEST_P(test_suite_name, test_name, match_core, match_product)                                                                          \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                                    \
    bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                                                                                               \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public test_suite_name {                                                                   \
      public:                                                                                                                                             \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                                \
        () {}                                                                                                                                             \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                                \
        (const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete;                                                                            \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                                \
        (GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) = delete;                                                                                 \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &operator=(const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete;               \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &operator=(GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) = delete;                    \
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
                        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)>(),                                                                            \
                    ::testing::internal::CodeLocation(__FILE__, __LINE__));                                                                               \
            return 0;                                                                                                                                     \
        }                                                                                                                                                 \
        static int gtest_registering_dummy_;                                                                                                              \
    };                                                                                                                                                    \
    int GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                                           \
                               test_name)::gtest_registering_dummy_ =                                                                                     \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();                                                                              \
    template <typename FamilyType>                                                                                                                        \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::testBodyHw()

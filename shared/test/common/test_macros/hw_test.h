/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/test_macros/hw_test_base.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"
#include "hw_cmds.h"
#include "neo_igfxfmid.h"
#include "per_product_test_selector.h"
#include "test_mode.h"

// Macros to provide template based testing.
// Test can use FamilyType in the test -- equivalent to Gen9Family
#define HWTEST_TEST_(test_suite_name, test_name, parent_class, parent_id, SetUpT_name, TearDownT_name)                                      \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                      \
                                                                                                                                            \
    bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                                                                                 \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                                                        \
                                                                                                                                            \
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
        template <typename T>                                                                                                               \
        void emptyFcn() {}                                                                                                                  \
        void SetUp() override {                                                                                                             \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                             \
                GTEST_SKIP();                                                                                                               \
            }                                                                                                                               \
            parent_class::SetUp();                                                                                                          \
            FAMILY_SELECTOR(::renderCoreFamily, SetUpT_name);                                                                               \
        }                                                                                                                                   \
        void TearDown() override {                                                                                                          \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                FAMILY_SELECTOR(::renderCoreFamily, TearDownT_name)                                                                         \
                parent_class::TearDown();                                                                                                   \
            }                                                                                                                               \
        }                                                                                                                                   \
                                                                                                                                            \
        void TestBody() override {                                                                                                          \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                FAMILY_SELECTOR(::renderCoreFamily, testBodyHw)                                                                             \
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

#define HWTEST_F(test_fixture, test_name)               \
    HWTEST_TEST_(test_fixture, test_name, test_fixture, \
                 ::testing::internal::GetTypeId<test_fixture>(), emptyFcn, emptyFcn)

// Macros to provide template based testing.
// Test can use productFamily and FamilyType in the test
#define HWTEST2_TEST_(test_suite_name, test_name, parent_class, parent_id, test_matcher)                                                    \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                      \
                                                                                                                                            \
    bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                                                                                 \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                                                        \
                                                                                                                                            \
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
        using MatcherType = test_matcher;                                                                                                   \
                                                                                                                                            \
        template <PRODUCT_FAMILY productFamily, typename FamilyType>                                                                        \
        void matchBody();                                                                                                                   \
                                                                                                                                            \
        template <unsigned int matcherOrdinal>                                                                                              \
        void checkForMatch(PRODUCT_FAMILY matchProduct);                                                                                    \
                                                                                                                                            \
        template <unsigned int matcherOrdinal>                                                                                              \
        bool checkMatch(PRODUCT_FAMILY matchProduct);                                                                                       \
                                                                                                                                            \
        void SetUp() override;                                                                                                              \
        void TearDown() override;                                                                                                           \
        void TestBody() override;                                                                                                           \
                                                                                                                                            \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                                                               \
    };                                                                                                                                      \
                                                                                                                                            \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =                                             \
        ::testing::internal::MakeAndRegisterTestInfo(                                                                                       \
            #test_suite_name, #test_name, nullptr, nullptr,                                                                                 \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                                                             \
            ::testing::internal::SuiteApiResolver<parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__),                                   \
            ::testing::internal::SuiteApiResolver<parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__),                                \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(test_suite_name,                                                \
                                                                            test_name)>);                                                   \
                                                                                                                                            \
    template <unsigned int matcherOrdinal>                                                                                                  \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                            \
                                test_name)::checkForMatch(PRODUCT_FAMILY matchProduct) {                                                    \
        constexpr PRODUCT_FAMILY productFamily = supportedProductFamilies[matcherOrdinal];                                                  \
                                                                                                                                            \
        if (matchProduct == productFamily) {                                                                                                \
            if constexpr (MatcherType::isMatched<productFamily>()) {                                                                        \
                using FamilyType = typename NEO::HwMapper<productFamily>::GfxFamily;                                                        \
                matchBody<productFamily, FamilyType>();                                                                                     \
            }                                                                                                                               \
        } else if constexpr (matcherOrdinal > 0) {                                                                                          \
            checkForMatch<matcherOrdinal - 1u>(matchProduct);                                                                               \
        }                                                                                                                                   \
    }                                                                                                                                       \
    template <unsigned int matcherOrdinal>                                                                                                  \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                            \
                                test_name)::checkMatch(PRODUCT_FAMILY matchProduct) {                                                       \
        constexpr PRODUCT_FAMILY productFamily = supportedProductFamilies[matcherOrdinal];                                                  \
                                                                                                                                            \
        if (matchProduct == productFamily) {                                                                                                \
            return MatcherType::isMatched<productFamily>();                                                                                 \
        } else if constexpr (matcherOrdinal > 0) {                                                                                          \
            return checkMatch<matcherOrdinal - 1u>(matchProduct);                                                                           \
        }                                                                                                                                   \
        return false;                                                                                                                       \
    }                                                                                                                                       \
                                                                                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::SetUp() {                                                                      \
        if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                 \
            GTEST_SKIP();                                                                                                                   \
        }                                                                                                                                   \
        if (checkMatch<supportedProductFamilies.size() - 1u>(::productFamily)) {                                                            \
            parent_class::SetUp();                                                                                                          \
        }                                                                                                                                   \
    }                                                                                                                                       \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TearDown() {                                                                   \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                \
            if (checkMatch<supportedProductFamilies.size() - 1u>(::productFamily)) {                                                        \
                parent_class::TearDown();                                                                                                   \
            }                                                                                                                               \
        }                                                                                                                                   \
    }                                                                                                                                       \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody() {                                                                   \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                \
            checkForMatch<supportedProductFamilies.size() - 1u>(::productFamily);                                                           \
        }                                                                                                                                   \
    }                                                                                                                                       \
                                                                                                                                            \
    template <PRODUCT_FAMILY productFamily, typename FamilyType>                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::matchBody()

// Compared to HWTEST2_TEST_ allows setup and teardown to be called with the FamilyType template
#define HWTEST2_TEMPLATED_(test_suite_name, test_name, parent_class, parent_id, test_matcher, SetUpT_name, TearDownT_name)                  \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                      \
                                                                                                                                            \
    bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                                                                                 \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                                                        \
                                                                                                                                            \
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
        using MatcherType = test_matcher;                                                                                                   \
                                                                                                                                            \
        template <PRODUCT_FAMILY productFamily, typename FamilyType>                                                                        \
        void matchBody();                                                                                                                   \
                                                                                                                                            \
        template <unsigned int matcherOrdinal>                                                                                              \
        void checkForMatch(PRODUCT_FAMILY matchProduct);                                                                                    \
                                                                                                                                            \
        template <unsigned int matcherOrdinal>                                                                                              \
        bool checkMatch(PRODUCT_FAMILY matchProduct);                                                                                       \
                                                                                                                                            \
        void SetUp() override;                                                                                                              \
        void TearDown() override;                                                                                                           \
        void TestBody() override;                                                                                                           \
                                                                                                                                            \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                                                               \
    };                                                                                                                                      \
                                                                                                                                            \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =                                             \
        ::testing::internal::MakeAndRegisterTestInfo(                                                                                       \
            #test_suite_name, #test_name, nullptr, nullptr,                                                                                 \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                                                             \
            ::testing::internal::SuiteApiResolver<parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__),                                   \
            ::testing::internal::SuiteApiResolver<parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__),                                \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(test_suite_name,                                                \
                                                                            test_name)>);                                                   \
                                                                                                                                            \
    template <unsigned int matcherOrdinal>                                                                                                  \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                            \
                                test_name)::checkForMatch(PRODUCT_FAMILY matchProduct) {                                                    \
        constexpr PRODUCT_FAMILY productFamily = supportedProductFamilies[matcherOrdinal];                                                  \
                                                                                                                                            \
        if (matchProduct == productFamily) {                                                                                                \
            if constexpr (MatcherType::isMatched<productFamily>()) {                                                                        \
                using FamilyType = typename NEO::HwMapper<productFamily>::GfxFamily;                                                        \
                matchBody<productFamily, FamilyType>();                                                                                     \
            }                                                                                                                               \
        } else if constexpr (matcherOrdinal > 0) {                                                                                          \
            checkForMatch<matcherOrdinal - 1u>(matchProduct);                                                                               \
        }                                                                                                                                   \
    }                                                                                                                                       \
    template <unsigned int matcherOrdinal>                                                                                                  \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                            \
                                test_name)::checkMatch(PRODUCT_FAMILY matchProduct) {                                                       \
        constexpr PRODUCT_FAMILY productFamily = supportedProductFamilies[matcherOrdinal];                                                  \
                                                                                                                                            \
        if (matchProduct == productFamily) {                                                                                                \
            return MatcherType::isMatched<productFamily>();                                                                                 \
        } else if constexpr (matcherOrdinal > 0) {                                                                                          \
            return checkMatch<matcherOrdinal - 1u>(matchProduct);                                                                           \
        }                                                                                                                                   \
        return false;                                                                                                                       \
    }                                                                                                                                       \
                                                                                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::SetUp() {                                                                      \
        if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                 \
            GTEST_SKIP();                                                                                                                   \
        }                                                                                                                                   \
        if (checkMatch<supportedProductFamilies.size() - 1u>(::productFamily)) {                                                            \
            parent_class::SetUp();                                                                                                          \
            FAMILY_SELECTOR(::renderCoreFamily, SetUpT_name)                                                                                \
        }                                                                                                                                   \
    }                                                                                                                                       \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TearDown() {                                                                   \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                \
            if (checkMatch<supportedProductFamilies.size() - 1u>(::productFamily)) {                                                        \
                FAMILY_SELECTOR(::renderCoreFamily, TearDownT_name)                                                                         \
                parent_class::TearDown();                                                                                                   \
            }                                                                                                                               \
        }                                                                                                                                   \
    }                                                                                                                                       \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody() {                                                                   \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                \
            checkForMatch<supportedProductFamilies.size() - 1u>(::productFamily);                                                           \
        }                                                                                                                                   \
    }                                                                                                                                       \
                                                                                                                                            \
    template <PRODUCT_FAMILY productFamily, typename FamilyType>                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::matchBody()

#define HWTEST2_F(test_fixture, test_name, test_matcher)                  \
    HWTEST2_TEST_(test_fixture, test_name##_##test_matcher, test_fixture, \
                  ::testing::internal::GetTypeId<test_fixture>(), test_matcher)

#define HWTEST_TEMPLATED_F(test_fixture, test_name)     \
    HWTEST_TEST_(test_fixture, test_name, test_fixture, \
                 ::testing::internal::GetTypeId<test_fixture>(), setUpT, tearDownT)

#define HWTEST2_TEMPLATED_F(test_fixture, test_name, test_matcher)             \
    HWTEST2_TEMPLATED_(test_fixture, test_name##_##test_matcher, test_fixture, \
                       ::testing::internal::GetTypeId<test_fixture>(), test_matcher, setUpT, tearDownT)

// Macros to provide template based testing.
// Test can use FamilyType in the test -- equivalent to Gen9Family
#define HWCMDTEST_TEST_(cmdset_gen_base, test_suite_name, test_name, parent_class, parent_id)                                               \
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
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)>                                   \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<ShouldBeTested>::type {                                                   \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                testBodyHw<FamilyType>();                                                                                                   \
            }                                                                                                                               \
        }                                                                                                                                   \
                                                                                                                                            \
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)>                                   \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<false == ShouldBeTested>::type {                                          \
            /* do nothing */                                                                                                                \
        }                                                                                                                                   \
                                                                                                                                            \
        void TestBody() override {                                                                                                          \
            FAMILY_SELECTOR(::renderCoreFamily, runCmdTestHwIfSupported)                                                                    \
        }                                                                                                                                   \
        void SetUp() override {                                                                                                             \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                             \
                GTEST_SKIP();                                                                                                               \
            }                                                                                                                               \
            CALL_IF_SUPPORTED(cmdset_gen_base, parent_class::SetUp());                                                                      \
        }                                                                                                                                   \
        void TearDown() override {                                                                                                          \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                CALL_IF_SUPPORTED(cmdset_gen_base, parent_class::TearDown());                                                               \
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

// Compared to HWCMDTEST_TEST_ allows setup and teardown to be called with the FamilyType template
#define HWCMDTEST_TEMPLATED_(cmdset_gen_base, test_suite_name, test_name, parent_class, parent_id, SetUpT_name, TearDownT_name)             \
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
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)>                                   \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<ShouldBeTested>::type {                                                   \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                testBodyHw<FamilyType>();                                                                                                   \
            }                                                                                                                               \
        }                                                                                                                                   \
                                                                                                                                            \
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)>                                   \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<false == ShouldBeTested>::type {                                          \
            /* do nothing */                                                                                                                \
        }                                                                                                                                   \
                                                                                                                                            \
        void TestBody() override {                                                                                                          \
            FAMILY_SELECTOR(::renderCoreFamily, runCmdTestHwIfSupported)                                                                    \
        }                                                                                                                                   \
        void SetUp() override {                                                                                                             \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                             \
                GTEST_SKIP();                                                                                                               \
            }                                                                                                                               \
            CALL_IF_SUPPORTED(cmdset_gen_base, parent_class::SetUp());                                                                      \
            FAMILY_SELECTOR(::renderCoreFamily, SetUpT_name);                                                                               \
        }                                                                                                                                   \
        void TearDown() override {                                                                                                          \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                            \
                FAMILY_SELECTOR(::renderCoreFamily, TearDownT_name);                                                                        \
                CALL_IF_SUPPORTED(cmdset_gen_base, parent_class::TearDown());                                                               \
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

#define HWCMDTEST_F(cmdset_gen_base, test_fixture, test_name)               \
    HWCMDTEST_TEST_(cmdset_gen_base, test_fixture, test_name, test_fixture, \
                    ::testing::internal::GetTypeId<test_fixture>())

#define HWCMDTEST_TEMPLATED_F(cmdset_gen_base, test_fixture, test_name)          \
    HWCMDTEST_TEMPLATED_(cmdset_gen_base, test_fixture, test_name, test_fixture, \
                         ::testing::internal::GetTypeId<test_fixture>(), setUpT, tearDownT)

// Equivalent Hw specific macro for permuted tests
// Test can use FamilyType in the test -- equivalent to Gen9Family
#define HWTEST_P(test_suite_name, test_name)                                                                                                              \
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
                FAMILY_SELECTOR(::renderCoreFamily, testBodyHw)                                                                                           \
            }                                                                                                                                             \
        }                                                                                                                                                 \
        void SetUp() override {                                                                                                                           \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                           \
                GTEST_SKIP();                                                                                                                             \
            }                                                                                                                                             \
            test_suite_name::SetUp();                                                                                                                     \
        }                                                                                                                                                 \
        void TearDown() override {                                                                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                          \
                test_suite_name::TearDown();                                                                                                              \
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

#define HWTEST_TEMPLATED_P(test_suite_name, test_name)                                                                                                    \
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
                FAMILY_SELECTOR(::renderCoreFamily, testBodyHw)                                                                                           \
            }                                                                                                                                             \
        }                                                                                                                                                 \
        void SetUp() override {                                                                                                                           \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                           \
                GTEST_SKIP();                                                                                                                             \
            }                                                                                                                                             \
            test_suite_name::SetUp();                                                                                                                     \
            FAMILY_SELECTOR(::renderCoreFamily, setUpT);                                                                                                  \
        }                                                                                                                                                 \
        void TearDown() override {                                                                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                          \
                FAMILY_SELECTOR(::renderCoreFamily, tearDownT)                                                                                            \
                test_suite_name::TearDown();                                                                                                              \
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

#define HWCMDTEST_P(cmdset_gen_base, test_suite_name, test_name)                                                                                          \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                                    \
                                                                                                                                                          \
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
                                                                                                                                                          \
        template <typename FamilyType>                                                                                                                    \
        void testBodyHw();                                                                                                                                \
                                                                                                                                                          \
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)>                                                 \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<ShouldBeTested>::type {                                                                 \
            testBodyHw<FamilyType>();                                                                                                                     \
        }                                                                                                                                                 \
                                                                                                                                                          \
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)>                                                 \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<false == ShouldBeTested>::type {                                                        \
            /* do nothing */                                                                                                                              \
        }                                                                                                                                                 \
                                                                                                                                                          \
        void TestBody() override {                                                                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                          \
                FAMILY_SELECTOR(::renderCoreFamily, runCmdTestHwIfSupported)                                                                              \
            }                                                                                                                                             \
        }                                                                                                                                                 \
        void SetUp() override {                                                                                                                           \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                           \
                GTEST_SKIP();                                                                                                                             \
            }                                                                                                                                             \
            CALL_IF_SUPPORTED(cmdset_gen_base, test_suite_name::SetUp());                                                                                 \
        }                                                                                                                                                 \
        void TearDown() override {                                                                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                          \
                CALL_IF_SUPPORTED(cmdset_gen_base, test_suite_name::TearDown());                                                                          \
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

// Macros to provide template based testing.
// Test can use productFamily, FamilyType in the test
#define HWTEST2_P(test_suite_name, test_name, test_matcher)                                                                                 \
    bool TEST_EXCLUDE_VARIABLE(test_suite_name, test_name);                                                                                 \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public test_suite_name {                                                     \
                                                                                                                                            \
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
        template <PRODUCT_FAMILY productFamily, typename FamilyType>                                                                        \
        void matchBody();                                                                                                                   \
                                                                                                                                            \
      private:                                                                                                                              \
        using MatcherType = test_matcher;                                                                                                   \
                                                                                                                                            \
        template <unsigned int matcherOrdinal>                                                                                              \
        void checkForMatch(PRODUCT_FAMILY matchProduct);                                                                                    \
                                                                                                                                            \
        template <unsigned int matcherOrdinal>                                                                                              \
        bool checkMatch(PRODUCT_FAMILY matchProduct);                                                                                       \
                                                                                                                                            \
        void SetUp() override;                                                                                                              \
        void TearDown() override;                                                                                                           \
                                                                                                                                            \
        void TestBody() override;                                                                                                           \
                                                                                                                                            \
        static int AddToRegistry() {                                                                                                        \
            ::testing::UnitTest::GetInstance()                                                                                              \
                ->parameterized_test_registry()                                                                                             \
                .GetTestCasePatternHolder<test_suite_name>(#test_suite_name,                                                                \
                                                           ::testing::internal::CodeLocation(__FILE__, __LINE__))                           \
                ->AddTestPattern(#test_suite_name, #test_name,                                                                              \
                                 new ::testing::internal::TestMetaFactory<GTEST_TEST_CLASS_NAME_(                                           \
                                     test_suite_name, test_name)>(),                                                                        \
                                 ::testing::internal::CodeLocation(__FILE__, __LINE__));                                                    \
            return 0;                                                                                                                       \
        }                                                                                                                                   \
        static int gtest_registering_dummy_;                                                                                                \
    };                                                                                                                                      \
                                                                                                                                            \
    template <unsigned int matcherOrdinal>                                                                                                  \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                            \
                                test_name)::checkForMatch(PRODUCT_FAMILY matchProduct) {                                                    \
        constexpr PRODUCT_FAMILY productFamily = supportedProductFamilies[matcherOrdinal];                                                  \
                                                                                                                                            \
        if (matchProduct == productFamily) {                                                                                                \
            if constexpr (MatcherType::isMatched<productFamily>()) {                                                                        \
                using FamilyType = typename NEO::HwMapper<productFamily>::GfxFamily;                                                        \
                matchBody<productFamily, FamilyType>();                                                                                     \
            }                                                                                                                               \
        } else if constexpr (matcherOrdinal > 0) {                                                                                          \
            checkForMatch<matcherOrdinal - 1u>(matchProduct);                                                                               \
        }                                                                                                                                   \
    }                                                                                                                                       \
                                                                                                                                            \
    template <unsigned int matcherOrdinal>                                                                                                  \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                                            \
                                test_name)::checkMatch(PRODUCT_FAMILY matchProduct) {                                                       \
        constexpr PRODUCT_FAMILY productFamily = supportedProductFamilies[matcherOrdinal];                                                  \
                                                                                                                                            \
        if (matchProduct == productFamily) {                                                                                                \
            return MatcherType::isMatched<productFamily>();                                                                                 \
        } else if constexpr (matcherOrdinal > 0) {                                                                                          \
            return checkMatch<matcherOrdinal - 1u>(matchProduct);                                                                           \
        }                                                                                                                                   \
        return false;                                                                                                                       \
    }                                                                                                                                       \
                                                                                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::SetUp() {                                                                      \
        if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                 \
            GTEST_SKIP();                                                                                                                   \
        }                                                                                                                                   \
        if (checkMatch<supportedProductFamilies.size() - 1u>(::productFamily)) {                                                            \
            test_suite_name::SetUp();                                                                                                       \
        }                                                                                                                                   \
    }                                                                                                                                       \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TearDown() {                                                                   \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                \
            if (checkMatch<supportedProductFamilies.size() - 1u>(::productFamily)) {                                                        \
                test_suite_name::TearDown();                                                                                                \
            }                                                                                                                               \
        }                                                                                                                                   \
    }                                                                                                                                       \
                                                                                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody() {                                                                   \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                                                \
            checkForMatch<supportedProductFamilies.size() - 1u>(::productFamily);                                                           \
        }                                                                                                                                   \
    }                                                                                                                                       \
                                                                                                                                            \
    int GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::gtest_registering_dummy_ =                                                      \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();                                                                \
    template <PRODUCT_FAMILY productFamily, typename FamilyType>                                                                            \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::matchBody()

#include "per_product_test_definitions.h"

#define HWTEST_TYPED_TEST(CaseName, TestName)                                                                  \
    CHECK_TEST_NAME_LENGTH(CaseName, TestName)                                                                 \
    template <typename gtest_TypeParam_>                                                                       \
    class GTEST_TEST_CLASS_NAME_(CaseName, TestName) : public CaseName<gtest_TypeParam_> {                     \
      private:                                                                                                 \
        typedef CaseName<gtest_TypeParam_> TestFixture;                                                        \
        typedef gtest_TypeParam_ TypeParam;                                                                    \
        template <typename FamilyType>                                                                         \
        void testBodyHw();                                                                                     \
                                                                                                               \
        void TestBody() override {                                                                             \
            FAMILY_SELECTOR(::renderCoreFamily, testBodyHw)                                                    \
        }                                                                                                      \
    };                                                                                                         \
    bool gtest_##CaseName##_##TestName##_registered_ GTEST_ATTRIBUTE_UNUSED_ =                                 \
        ::testing::internal::TypeParameterizedTest<                                                            \
            CaseName,                                                                                          \
            ::testing::internal::TemplateSel<                                                                  \
                GTEST_TEST_CLASS_NAME_(CaseName, TestName)>,                                                   \
            GTEST_TYPE_PARAMS_(CaseName)>::Register("", ::testing::internal::CodeLocation(__FILE__, __LINE__), \
                                                    #CaseName, #TestName, 0);                                  \
    template <typename gtest_TypeParam_>                                                                       \
    template <typename FamilyType>                                                                             \
    void GTEST_TEST_CLASS_NAME_(CaseName, TestName)<gtest_TypeParam_>::testBodyHw()

[[maybe_unused]] static constexpr std::array supportedProductFamilies = {SUPPORTED_TEST_PRODUCT_FAMILIES};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() == gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsNotGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() != gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily, GFXCORE_FAMILY gfxCoreFamily2>
struct AreNotGfxCores {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() != gfxCoreFamily && NEO::ToGfxCoreFamily<productFamily>::get() != gfxCoreFamily2;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsAtMostGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() <= gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsAtLeastGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() >= gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamilyMin, GFXCORE_FAMILY gfxCoreFamilyMax>
struct IsWithinGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() >= gfxCoreFamilyMin && NEO::ToGfxCoreFamily<productFamily>::get() <= gfxCoreFamilyMax;
    }
};

template <GFXCORE_FAMILY gfxCoreFamilyMin, GFXCORE_FAMILY gfxCoreFamilyMax>
struct IsNotWithinGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() < gfxCoreFamilyMin || NEO::ToGfxCoreFamily<productFamily>::get() > gfxCoreFamilyMax;
    }
};

template <GFXCORE_FAMILY... args>
struct IsAnyGfxCores {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... || IsGfxCore<args>::template isMatched<productFamily>());
    }
};

template <GFXCORE_FAMILY... args>
struct IsNotAnyGfxCores {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... && IsNotGfxCore<args>::template isMatched<productFamily>());
    }
};

template <PRODUCT_FAMILY product>
struct IsProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily == product;
    }
};

template <PRODUCT_FAMILY productFamilyMax>
struct IsAtMostProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily <= productFamilyMax;
    }
};

template <PRODUCT_FAMILY productFamilyMin>
struct IsAtLeastProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= productFamilyMin;
    }
};

template <PRODUCT_FAMILY productFamilyMin, PRODUCT_FAMILY productFamilyMax>
struct IsWithinProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= productFamilyMin && productFamily <= productFamilyMax;
    }
};

template <PRODUCT_FAMILY productFamilyMin, PRODUCT_FAMILY productFamilyMax>
struct IsNotWithinProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (productFamily < productFamilyMin) || (productFamily > productFamilyMax);
    }
};

template <PRODUCT_FAMILY... args>
struct IsAnyProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... || IsProduct<args>::template isMatched<productFamily>());
    }
};

template <PRODUCT_FAMILY... args>
struct IsNoneProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... && !(IsProduct<args>::template isMatched<productFamily>()));
    }
};

struct MatchAny {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() { return true; }
};

struct SupportsSampler {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};

struct HeapfulSupportedMatch {

    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        [[maybe_unused]] const GFXCORE_FAMILY gfxCoreFamily = NEO::ToGfxCoreFamily<productFamily>::get();
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        constexpr bool heaplessModeEnabled = FamilyType::template isHeaplessMode<DefaultWalkerType>();
        return !heaplessModeEnabled;
    }
};

#include "common_matchers.h"

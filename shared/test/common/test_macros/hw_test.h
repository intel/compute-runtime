/*
 * Copyright (C) 2021-2022 Intel Corporation
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
#include "igfxfmid.h"
#include "per_product_test_selector.h"
#include "test_mode.h"

// Macros to provide template based testing.
// Test can use FamilyType in the test -- equivalent to Gen9Family
#define HWTEST_TEST_(test_suite_name, test_name, parent_class, parent_id, SetUpT_name, TearDownT_name) \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                 \
                                                                                                       \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                   \
                                                                                                       \
      public:                                                                                          \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                             \
        () {}                                                                                          \
                                                                                                       \
      private:                                                                                         \
        template <typename FamilyType>                                                                 \
        void testBodyHw();                                                                             \
        template <typename T>                                                                          \
        void emptyFcn() {}                                                                             \
        void SetUp() override {                                                                        \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                        \
                GTEST_SKIP();                                                                          \
            }                                                                                          \
            parent_class::SetUp();                                                                     \
            FAMILY_SELECTOR(::renderCoreFamily, SetUpT_name);                                          \
        }                                                                                              \
        void TearDown() override {                                                                     \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                       \
                FAMILY_SELECTOR(::renderCoreFamily, TearDownT_name)                                    \
                parent_class::TearDown();                                                              \
            }                                                                                          \
        }                                                                                              \
                                                                                                       \
        void TestBody() override {                                                                     \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                       \
                FAMILY_SELECTOR(::renderCoreFamily, testBodyHw)                                        \
            }                                                                                          \
        }                                                                                              \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                          \
        GTEST_DISALLOW_COPY_AND_ASSIGN_(                                                               \
            GTEST_TEST_CLASS_NAME_(test_suite_name, test_name));                                       \
    };                                                                                                 \
                                                                                                       \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =        \
        ::testing::internal::MakeAndRegisterTestInfo(                                                  \
            #test_suite_name, #test_name, nullptr, nullptr,                                            \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                        \
            ::testing::internal::SuiteApiResolver<                                                     \
                parent_class>::GetSetUpCaseOrSuite(),                                                  \
            ::testing::internal::SuiteApiResolver<                                                     \
                parent_class>::GetTearDownCaseOrSuite(),                                               \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(                           \
                test_suite_name, test_name)>);                                                         \
    template <typename FamilyType>                                                                     \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::testBodyHw()

#define HWTEST_F(test_fixture, test_name)               \
    HWTEST_TEST_(test_fixture, test_name, test_fixture, \
                 ::testing::internal::GetTypeId<test_fixture>(), emptyFcn, emptyFcn)

// Macros to provide template based testing.
// Test can use productFamily, gfxCoreFamily and FamilyType in the test
#define HWTEST2_TEST_(test_suite_name, test_name, parent_class, parent_id, test_matcher)           \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                             \
                                                                                                   \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {               \
                                                                                                   \
      public:                                                                                      \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                         \
        () {}                                                                                      \
                                                                                                   \
      private:                                                                                     \
        using ContainerType = SupportedProductFamilies;                                            \
        using MatcherType = test_matcher;                                                          \
                                                                                                   \
        template <PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCoreFamily, typename FamilyType> \
        void matchBody();                                                                          \
                                                                                                   \
        template <PRODUCT_FAMILY productFamily>                                                    \
        void matched() {                                                                           \
            const GFXCORE_FAMILY gfxCoreFamily =                                                   \
                static_cast<GFXCORE_FAMILY>(NEO::HwMapper<productFamily>::gfxFamily);              \
            using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;            \
            matchBody<productFamily, gfxCoreFamily, FamilyType>();                                 \
        }                                                                                          \
                                                                                                   \
        struct MatcherFalse {                                                                      \
            template <PRODUCT_FAMILY productFamily>                                                \
            static void matched() {}                                                               \
        };                                                                                         \
                                                                                                   \
        template <unsigned int matcherOrdinal>                                                     \
        void checkForMatch(PRODUCT_FAMILY matchProduct);                                           \
                                                                                                   \
        template <unsigned int matcherOrdinal>                                                     \
        bool checkMatch(PRODUCT_FAMILY matchProduct);                                              \
                                                                                                   \
        void SetUp() override;                                                                     \
        void TearDown() override;                                                                  \
        void TestBody() override;                                                                  \
                                                                                                   \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                      \
        GTEST_DISALLOW_COPY_AND_ASSIGN_(GTEST_TEST_CLASS_NAME_(test_suite_name, test_name));       \
    };                                                                                             \
                                                                                                   \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =    \
        ::testing::internal::MakeAndRegisterTestInfo(                                              \
            #test_suite_name, #test_name, nullptr, nullptr,                                        \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                    \
            ::testing::internal::SuiteApiResolver<parent_class>::GetSetUpCaseOrSuite(),            \
            ::testing::internal::SuiteApiResolver<parent_class>::GetTearDownCaseOrSuite(),         \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(test_suite_name,       \
                                                                            test_name)>);          \
                                                                                                   \
    template <unsigned int matcherOrdinal>                                                         \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                   \
                                test_name)::checkForMatch(PRODUCT_FAMILY matchProduct) {           \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;     \
                                                                                                   \
        if (matchProduct == productFamily) {                                                       \
            const bool isMatched = MatcherType::isMatched<productFamily>();                        \
            using Matcher =                                                                        \
                typename std::conditional<isMatched,                                               \
                                          GTEST_TEST_CLASS_NAME_(test_suite_name, test_name),      \
                                          MatcherFalse>::type;                                     \
            Matcher::template matched<productFamily>();                                            \
        } else {                                                                                   \
            checkForMatch<matcherOrdinal - 1u>(matchProduct);                                      \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    template <>                                                                                    \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                   \
                                test_name)::checkForMatch<0u>(PRODUCT_FAMILY matchProduct) {       \
        const int matcherOrdinal = 0u;                                                             \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;     \
                                                                                                   \
        if (matchProduct == productFamily) {                                                       \
            const bool isMatched = MatcherType::isMatched<productFamily>();                        \
            using Matcher =                                                                        \
                typename std::conditional<isMatched,                                               \
                                          GTEST_TEST_CLASS_NAME_(test_suite_name, test_name),      \
                                          MatcherFalse>::type;                                     \
            Matcher::template matched<productFamily>();                                            \
        }                                                                                          \
    }                                                                                              \
    template <unsigned int matcherOrdinal>                                                         \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                   \
                                test_name)::checkMatch(PRODUCT_FAMILY matchProduct) {              \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;     \
                                                                                                   \
        if (matchProduct == productFamily) {                                                       \
            const bool isMatched = MatcherType::isMatched<productFamily>();                        \
            return isMatched;                                                                      \
        } else {                                                                                   \
            return checkMatch<matcherOrdinal - 1u>(matchProduct);                                  \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    template <>                                                                                    \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                   \
                                test_name)::checkMatch<0>(PRODUCT_FAMILY matchProduct) {           \
        const int matcherOrdinal = 0u;                                                             \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;     \
                                                                                                   \
        if (matchProduct == productFamily) {                                                       \
            const bool isMatched = MatcherType::isMatched<productFamily>();                        \
            return isMatched;                                                                      \
        } else {                                                                                   \
            return false;                                                                          \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::SetUp() {                             \
        if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                        \
            GTEST_SKIP();                                                                          \
        }                                                                                          \
        if (checkMatch<SupportedProductFamilies::size - 1u>(::productFamily)) {                    \
            parent_class::SetUp();                                                                 \
        }                                                                                          \
    }                                                                                              \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TearDown() {                          \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                       \
            if (checkMatch<SupportedProductFamilies::size - 1u>(::productFamily)) {                \
                parent_class::TearDown();                                                          \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody() {                          \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                       \
            checkForMatch<SupportedProductFamilies::size - 1u>(::productFamily);                   \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    template <PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCoreFamily, typename FamilyType>     \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::matchBody()

#define HWTEST2_F(test_fixture, test_name, test_matcher)                  \
    HWTEST2_TEST_(test_fixture, test_name##_##test_matcher, test_fixture, \
                  ::testing::internal::GetTypeId<test_fixture>(), test_matcher)

#define HWTEST_TEMPLATED_F(test_fixture, test_name)     \
    HWTEST_TEST_(test_fixture, test_name, test_fixture, \
                 ::testing::internal::GetTypeId<test_fixture>(), setUpT, tearDownT)

// Macros to provide template based testing.
// Test can use FamilyType in the test -- equivalent to Gen9Family
#define HWCMDTEST_TEST_(cmdset_gen_base, test_suite_name, test_name, parent_class, parent_id)             \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                    \
                                                                                                          \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public parent_class {                      \
      public:                                                                                             \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                \
        () {}                                                                                             \
                                                                                                          \
      private:                                                                                            \
        template <typename FamilyType>                                                                    \
        void testBodyHw();                                                                                \
                                                                                                          \
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)> \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<ShouldBeTested>::type {                 \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                          \
                testBodyHw<FamilyType>();                                                                 \
            }                                                                                             \
        }                                                                                                 \
                                                                                                          \
        template <typename FamilyType, bool ShouldBeTested = FamilyType::supportsCmdSet(cmdset_gen_base)> \
        auto runCmdTestHwIfSupported() -> typename std::enable_if<false == ShouldBeTested>::type {        \
            /* do nothing */                                                                              \
        }                                                                                                 \
                                                                                                          \
        void TestBody() override {                                                                        \
            FAMILY_SELECTOR(::renderCoreFamily, runCmdTestHwIfSupported)                                  \
        }                                                                                                 \
        void SetUp() override {                                                                           \
            if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                           \
                GTEST_SKIP();                                                                             \
            }                                                                                             \
            CALL_IF_SUPPORTED(cmdset_gen_base, parent_class::SetUp());                                    \
        }                                                                                                 \
        void TearDown() override {                                                                        \
            if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                          \
                CALL_IF_SUPPORTED(cmdset_gen_base, parent_class::TearDown());                             \
            }                                                                                             \
        }                                                                                                 \
        static ::testing::TestInfo *const test_info_ GTEST_ATTRIBUTE_UNUSED_;                             \
        GTEST_DISALLOW_COPY_AND_ASSIGN_(                                                                  \
            GTEST_TEST_CLASS_NAME_(test_suite_name, test_name));                                          \
    };                                                                                                    \
                                                                                                          \
    ::testing::TestInfo *const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::test_info_ =           \
        ::testing::internal::MakeAndRegisterTestInfo(                                                     \
            #test_suite_name, #test_name, nullptr, nullptr,                                               \
            ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),                           \
            ::testing::internal::SuiteApiResolver<                                                        \
                parent_class>::GetSetUpCaseOrSuite(),                                                     \
            ::testing::internal::SuiteApiResolver<                                                        \
                parent_class>::GetTearDownCaseOrSuite(),                                                  \
            new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(                              \
                test_suite_name, test_name)>);                                                            \
    template <typename FamilyType>                                                                        \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::testBodyHw()

#define HWCMDTEST_F(cmdset_gen_base, test_fixture, test_name)               \
    HWCMDTEST_TEST_(cmdset_gen_base, test_fixture, test_name, test_fixture, \
                    ::testing::internal::GetTypeId<test_fixture>())

// Equivalent Hw specific macro for permuted tests
// Test can use FamilyType in the test -- equivalent to Gen9Family
#define HWTEST_P(test_suite_name, test_name)                                                                                                              \
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

#define HWCMDTEST_P(cmdset_gen_base, test_suite_name, test_name)                                                                                          \
    CHECK_TEST_NAME_LENGTH(test_suite_name, test_name)                                                                                                    \
                                                                                                                                                          \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public test_suite_name {                                                                   \
      public:                                                                                                                                             \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                                                                \
        () {}                                                                                                                                             \
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

// Macros to provide template based testing.
// Test can use productFamily, gfxCoreFamily and FamilyType in the test
#define HWTEST2_P(test_suite_name, test_name, test_matcher)                                                       \
    class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) : public test_suite_name {                           \
                                                                                                                  \
      public:                                                                                                     \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                                                        \
        () {}                                                                                                     \
                                                                                                                  \
        template <PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCoreFamily, typename FamilyType>                \
        void matchBody();                                                                                         \
                                                                                                                  \
      private:                                                                                                    \
        using ContainerType = SupportedProductFamilies;                                                           \
        using MatcherType = test_matcher;                                                                         \
                                                                                                                  \
        template <PRODUCT_FAMILY productFamily>                                                                   \
        void matched() {                                                                                          \
            const GFXCORE_FAMILY gfxCoreFamily =                                                                  \
                static_cast<GFXCORE_FAMILY>(NEO::HwMapper<productFamily>::gfxFamily);                             \
            using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;                           \
            matchBody<productFamily, gfxCoreFamily, FamilyType>();                                                \
        }                                                                                                         \
                                                                                                                  \
        struct MatcherFalse {                                                                                     \
            template <PRODUCT_FAMILY productFamily>                                                               \
            static void matched() {}                                                                              \
        };                                                                                                        \
                                                                                                                  \
        template <unsigned int matcherOrdinal>                                                                    \
        void checkForMatch(PRODUCT_FAMILY matchProduct);                                                          \
                                                                                                                  \
        template <unsigned int matcherOrdinal>                                                                    \
        bool checkMatch(PRODUCT_FAMILY matchProduct);                                                             \
                                                                                                                  \
        void SetUp() override;                                                                                    \
        void TearDown() override;                                                                                 \
                                                                                                                  \
        void TestBody() override;                                                                                 \
                                                                                                                  \
        static int AddToRegistry() {                                                                              \
            ::testing::UnitTest::GetInstance()                                                                    \
                ->parameterized_test_registry()                                                                   \
                .GetTestCasePatternHolder<test_suite_name>(#test_suite_name,                                      \
                                                           ::testing::internal::CodeLocation(__FILE__, __LINE__)) \
                ->AddTestPattern(#test_suite_name, #test_name,                                                    \
                                 new ::testing::internal::TestMetaFactory<GTEST_TEST_CLASS_NAME_(                 \
                                     test_suite_name, test_name)>());                                             \
            return 0;                                                                                             \
        }                                                                                                         \
        static int gtest_registering_dummy_;                                                                      \
        GTEST_DISALLOW_COPY_AND_ASSIGN_(GTEST_TEST_CLASS_NAME_(test_suite_name, test_name));                      \
    };                                                                                                            \
                                                                                                                  \
    template <unsigned int matcherOrdinal>                                                                        \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                  \
                                test_name)::checkForMatch(PRODUCT_FAMILY matchProduct) {                          \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;                    \
                                                                                                                  \
        if (matchProduct == productFamily) {                                                                      \
            const bool isMatched = MatcherType::isMatched<productFamily>();                                       \
            using Matcher =                                                                                       \
                typename std::conditional<isMatched,                                                              \
                                          GTEST_TEST_CLASS_NAME_(test_suite_name, test_name),                     \
                                          MatcherFalse>::type;                                                    \
            Matcher::template matched<productFamily>();                                                           \
        } else {                                                                                                  \
            checkForMatch<matcherOrdinal - 1u>(matchProduct);                                                     \
        }                                                                                                         \
    }                                                                                                             \
                                                                                                                  \
    template <>                                                                                                   \
    void GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                  \
                                test_name)::checkForMatch<0u>(PRODUCT_FAMILY matchProduct) {                      \
        const int matcherOrdinal = 0u;                                                                            \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;                    \
                                                                                                                  \
        if (matchProduct == productFamily) {                                                                      \
            const bool isMatched = MatcherType::isMatched<productFamily>();                                       \
            using Matcher =                                                                                       \
                typename std::conditional<isMatched,                                                              \
                                          GTEST_TEST_CLASS_NAME_(test_suite_name, test_name),                     \
                                          MatcherFalse>::type;                                                    \
            Matcher::template matched<productFamily>();                                                           \
        }                                                                                                         \
    }                                                                                                             \
    template <unsigned int matcherOrdinal>                                                                        \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                  \
                                test_name)::checkMatch(PRODUCT_FAMILY matchProduct) {                             \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;                    \
                                                                                                                  \
        if (matchProduct == productFamily) {                                                                      \
            const bool isMatched = MatcherType::isMatched<productFamily>();                                       \
            return isMatched;                                                                                     \
        } else {                                                                                                  \
            return checkMatch<matcherOrdinal - 1u>(matchProduct);                                                 \
        }                                                                                                         \
    }                                                                                                             \
                                                                                                                  \
    template <>                                                                                                   \
    bool GTEST_TEST_CLASS_NAME_(test_suite_name,                                                                  \
                                test_name)::checkMatch<0>(PRODUCT_FAMILY matchProduct) {                          \
        const int matcherOrdinal = 0u;                                                                            \
        const PRODUCT_FAMILY productFamily = At<ContainerType, matcherOrdinal>::productFamily;                    \
                                                                                                                  \
        if (matchProduct == productFamily) {                                                                      \
            const bool isMatched = MatcherType::isMatched<productFamily>();                                       \
            return isMatched;                                                                                     \
        } else {                                                                                                  \
            return false;                                                                                         \
        }                                                                                                         \
    }                                                                                                             \
                                                                                                                  \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::SetUp() {                                            \
        if (IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                       \
            GTEST_SKIP();                                                                                         \
        }                                                                                                         \
        if (checkMatch<SupportedProductFamilies::size - 1u>(::productFamily)) {                                   \
            test_suite_name::SetUp();                                                                             \
        }                                                                                                         \
    }                                                                                                             \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TearDown() {                                         \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                      \
            if (checkMatch<SupportedProductFamilies::size - 1u>(::productFamily)) {                               \
                test_suite_name::TearDown();                                                                      \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
                                                                                                                  \
    void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody() {                                         \
        if (!IS_TEST_EXCLUDED(test_suite_name, test_name)) {                                                      \
            checkForMatch<SupportedProductFamilies::size - 1u>(::productFamily);                                  \
        }                                                                                                         \
    }                                                                                                             \
                                                                                                                  \
    int GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::gtest_registering_dummy_ =                            \
        GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();                                      \
    template <PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCoreFamily, typename FamilyType>                    \
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

template <PRODUCT_FAMILY... args>
struct SupportedProductFamilyContainer {
    using BaseClass = SupportedProductFamilyContainer<args...>;
    static const PRODUCT_FAMILY productFamily = IGFX_UNKNOWN;
};

template <PRODUCT_FAMILY p, PRODUCT_FAMILY... args>
struct SupportedProductFamilyContainer<p, args...> : SupportedProductFamilyContainer<args...> {
    using BaseClass = SupportedProductFamilyContainer<args...>;

    static const PRODUCT_FAMILY productFamily = p;
    static const std::size_t size = sizeof...(args) + 1;
};

using SupportedProductFamilies =
    SupportedProductFamilyContainer<SUPPORTED_TEST_PRODUCT_FAMILIES>;

// Static container accessor
template <typename Container, int index>
struct At {
    static const PRODUCT_FAMILY productFamily =
        At<typename Container::BaseClass, index - 1>::productFamily;
};

template <typename Container>
struct At<Container, 0> {
    static const PRODUCT_FAMILY productFamily = Container::productFamily;
};

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
struct IsBeforeGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() < gfxCoreFamily;
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

#include "common_matchers.h"

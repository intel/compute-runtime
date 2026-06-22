/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy_option_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

class CachePolicyOptionHelperTest : public ::testing::Test {
};

TEST_F(CachePolicyOptionHelperTest, WhenReplacingCachePolicyWithDifferentLengthsThenEntireOldPolicyIsRemoved) {
    std::string buildOptions = "-cl-store-cache-default=1234 -cl-load-cache-default=34";
    const char *currentCachePolicy = "-cl-store-cache-default=22 -cl-load-cache-default=66";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=22 -cl-load-cache-default=66", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenReplacingWithLongerPolicyThenOldPolicyFullyReplaced) {
    std::string buildOptions = "-cl-store-cache-default=1 -cl-load-cache-default=2";
    const char *currentCachePolicy = "-cl-store-cache-default=100 -cl-load-cache-default=200";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=100 -cl-load-cache-default=200", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenReplacingWithShorterPolicyThenOldPolicyFullyReplaced) {
    std::string buildOptions = "-cl-store-cache-default=1234567 -cl-load-cache-default=7654321";
    const char *currentCachePolicy = "-cl-store-cache-default=1 -cl-load-cache-default=2";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=1 -cl-load-cache-default=2", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenReplacingCachePolicyWithPrefixOnlyThenOnlyFirstOptionReplaced) {
    std::string buildOptions = "-cl-store-cache-default=1234 -cl-load-cache-default=34";
    const char *currentCachePolicy = "-cl-store-cache-default=22";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=22", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenCachePolicyIsOnlyThingInStringThenReplaceEntireString) {
    std::string buildOptions = "-cl-store-cache-default=1234 -cl-load-cache-default=34";
    const char *currentCachePolicy = "-cl-store-cache-default=99 -cl-load-cache-default=88";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=99 -cl-load-cache-default=88", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenCachePolicyIsAtBeginningWithOtherOptionsAfterThenOnlyReplaceCachePolicy) {
    std::string buildOptions = "-cl-store-cache-default=1234 -cl-load-cache-default=34 -O3 -other-option";
    const char *currentCachePolicy = "-cl-store-cache-default=22 -cl-load-cache-default=66";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=22 -cl-load-cache-default=66 -O3 -other-option", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenOnlyStoreOptionPresentThenReplaceOnlyStoreOption) {
    std::string buildOptions = "-cl-store-cache-default=1234 -O3";
    const char *currentCachePolicy = "-cl-store-cache-default=22";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=22 -O3", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenNoCachePolicyFoundThenStringUnchanged) {
    std::string buildOptions = "-O3 -other-option";
    const char *currentCachePolicy = "-cl-store-cache-default=22 -cl-load-cache-default=66";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-O3 -other-option", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenCachePolicyHasNoSpaceAfterThenReplaceUntilEnd) {
    std::string buildOptions = "-cl-store-cache-default=1234 -cl-load-cache-default=34";
    const char *currentCachePolicy = "-cl-store-cache-default=99 -cl-load-cache-default=88";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);

    EXPECT_EQ("-cl-store-cache-default=99 -cl-load-cache-default=88", buildOptions);
}

TEST_F(CachePolicyOptionHelperTest, WhenReplacingMultipleTimesEachReplacementIsCorrect) {
    std::string buildOptions = "-cl-store-cache-default=1234 -cl-load-cache-default=34";
    const char *policy1 = "-cl-store-cache-default=22 -cl-load-cache-default=66";
    const char *policy2 = "-cl-store-cache-default=99 -cl-load-cache-default=88";

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, policy1);
    EXPECT_EQ("-cl-store-cache-default=22 -cl-load-cache-default=66", buildOptions);

    CachePolicyOptionHelper::replaceCachePolicy(buildOptions, policy2);
    EXPECT_EQ("-cl-store-cache-default=99 -cl-load-cache-default=88", buildOptions);
}

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_matcher_registry.h"

#include "shared/test/common/test_macros/test_base.h"
#include "shared/test/common/test_macros/test_excludes.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

using namespace NEO;

using MatchersPerTest = std::unordered_map<std::string, TestMatcherRegistry::MatchFunc>;

static std::unique_ptr<MatchersPerTest> pMatchersPerTest;

void TestMatcherRegistry::registerMatcher(const char *testName, MatchFunc matchFunc) {
    if (pMatchersPerTest == nullptr) {
        pMatchersPerTest = std::make_unique<MatchersPerTest>();
    }
    pMatchersPerTest->emplace(testName, matchFunc);
}

bool TestMatcherRegistry::willRunForCurrentProduct(const std::string &testName, const PRODUCT_FAMILY productFamily) {
    if (pMatchersPerTest == nullptr) {
        return true;
    }
    auto it = pMatchersPerTest->find(testName);
    if (it == pMatchersPerTest->end()) {
        return true;
    }
    return it->second(productFamily);
}

bool TestMatcherRegistry::willRunForCurrentProduct(const ::testing::TestInfo &testInfo, const PRODUCT_FAMILY productFamily) {
    std::string_view suiteName = testInfo.test_suite_name();
    auto suiteSlash = suiteName.rfind('/');
    if (suiteSlash != std::string_view::npos) {
        suiteName.remove_prefix(suiteSlash + 1);
    }

    std::string_view testName = testInfo.name();
    auto testSlash = testName.rfind('/');
    if (testSlash != std::string_view::npos) {
        testName.remove_suffix(testName.size() - testSlash);
    }

    std::string testCaseName;
    testCaseName.reserve(suiteName.size() + testName.size());
    testCaseName.append(suiteName).append(testName);
    if (NEO::TestExcludes::isTestExcluded(testCaseName, productFamily, ::renderCoreFamily)) {
        return false;
    }
    return willRunForCurrentProduct(testCaseName, productFamily);
}

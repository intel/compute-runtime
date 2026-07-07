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

#include <map>
#include <memory>
#include <string>

using namespace NEO;

static std::unique_ptr<std::map<std::string, TestMatcherRegistry::MatchFunc>> pMatchersPerTest;

void TestMatcherRegistry::registerMatcher(const char *testName, MatchFunc matchFunc) {
    if (pMatchersPerTest == nullptr) {
        pMatchersPerTest = std::make_unique<std::map<std::string, MatchFunc>>();
    }
    pMatchersPerTest->emplace(testName, matchFunc);
}

bool TestMatcherRegistry::willRunForCurrentProduct(const char *testName, const PRODUCT_FAMILY productFamily) {
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
    std::string suiteName = testInfo.test_suite_name();
    auto suiteSlash = suiteName.rfind('/');
    if (suiteSlash != std::string::npos) {
        suiteName = suiteName.substr(suiteSlash + 1);
    }

    std::string testName = testInfo.name();
    auto testSlash = testName.rfind('/');
    if (testSlash != std::string::npos) {
        testName = testName.substr(0, testSlash);
    }

    auto testCaseName = suiteName + testName;
    if (NEO::TestExcludes::isTestExcluded(testCaseName.c_str(), productFamily, ::renderCoreFamily)) {
        return false;
    }
    return willRunForCurrentProduct(testCaseName.c_str(), productFamily);
}

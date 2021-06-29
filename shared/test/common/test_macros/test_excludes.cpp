/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

#include "shared/source/helpers/debug_helpers.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

using namespace NEO;

static std::unique_ptr<std::map<std::string, std::unordered_set<uint32_t>>> pExcludesPerTest;

bool TestExcludes::isTestExcluded(const char *testName, const uint32_t product) {
    if ((pExcludesPerTest == nullptr) || pExcludesPerTest->count(testName) == 0) {
        return false;
    }
    return (pExcludesPerTest->at(testName).count(product) > 0);
}

void TestExcludes::addTestExclude(const char *testName, const uint32_t product) {
    if (pExcludesPerTest == nullptr) {
        pExcludesPerTest = std::make_unique<std::map<std::string, std::unordered_set<uint32_t>>>();
    }
    if (pExcludesPerTest->count(testName) == 0) {
        pExcludesPerTest->insert(std::make_pair(testName, std::unordered_set<uint32_t>{}));
    }
    DEBUG_BREAK_IF(pExcludesPerTest->at(testName).count(product) != 0);
    pExcludesPerTest->at(testName).insert(product);
}

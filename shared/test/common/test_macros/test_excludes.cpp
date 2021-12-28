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

PRODUCT_FAMILY productFamily = {};
GFXCORE_FAMILY renderCoreFamily = {};

static std::unique_ptr<std::map<std::string, std::unordered_set<uint32_t>>> pProductExcludesPerTest;
static std::unique_ptr<std::map<std::string, std::unordered_set<uint32_t>>> pGfxExcludesPerTest;

bool isExcluded(const char *testName, const uint32_t family, std::unique_ptr<std::map<std::string, std::unordered_set<uint32_t>>> &pExcludesPerTest) {
    if ((pExcludesPerTest == nullptr) || pExcludesPerTest->count(testName) == 0) {
        return false;
    }
    return (pExcludesPerTest->at(testName).count(family) > 0);
}

void addExclude(const char *testName, const uint32_t family, std::unique_ptr<std::map<std::string, std::unordered_set<uint32_t>>> &pExcludesPerTest) {
    if (pExcludesPerTest == nullptr) {
        pExcludesPerTest = std::make_unique<std::map<std::string, std::unordered_set<uint32_t>>>();
    }
    if (pExcludesPerTest->count(testName) == 0) {
        pExcludesPerTest->insert(std::make_pair(testName, std::unordered_set<uint32_t>{}));
    }
    pExcludesPerTest->at(testName).insert(family);
}

bool TestExcludes::isTestExcluded(const char *testName, const PRODUCT_FAMILY productFamily, const GFXCORE_FAMILY gfxFamily) {
    return (isExcluded(testName, productFamily, pProductExcludesPerTest) || isExcluded(testName, gfxFamily, pGfxExcludesPerTest));
}

void TestExcludes::addTestExclude(const char *testName, const PRODUCT_FAMILY family) {
    addExclude(testName, family, pProductExcludesPerTest);
}

void TestExcludes::addTestExclude(const char *testName, const GFXCORE_FAMILY family) {
    addExclude(testName, family, pGfxExcludesPerTest);
}

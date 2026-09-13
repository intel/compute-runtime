/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace NEO;

PRODUCT_FAMILY productFamily = {};
GFXCORE_FAMILY renderCoreFamily = {};

using ExcludesPerTest = std::unordered_map<std::string, std::unordered_set<uint32_t>>;

static std::unique_ptr<ExcludesPerTest> pProductExcludesPerTest;
static std::unique_ptr<ExcludesPerTest> pGfxExcludesPerTest;

bool isExcluded(const std::string &testName, const uint32_t family, std::unique_ptr<ExcludesPerTest> &pExcludesPerTest) {
    if (pExcludesPerTest == nullptr) {
        return false;
    }
    auto it = pExcludesPerTest->find(testName);
    return (it != pExcludesPerTest->end()) && it->second.contains(family);
}

void addExclude(const char *testName, const uint32_t family, std::unique_ptr<ExcludesPerTest> &pExcludesPerTest) {
    if (pExcludesPerTest == nullptr) {
        pExcludesPerTest = std::make_unique<ExcludesPerTest>();
    }
    (*pExcludesPerTest)[testName].insert(family);
}

bool TestExcludes::isTestExcluded(const std::string &testName, const PRODUCT_FAMILY productFamily, const GFXCORE_FAMILY gfxFamily) {
    return (isExcluded(testName, productFamily, pProductExcludesPerTest) || isExcluded(testName, gfxFamily, pGfxExcludesPerTest));
}

void TestExcludes::addTestExclude(const char *testName, const PRODUCT_FAMILY family) {
    addExclude(testName, family, pProductExcludesPerTest);
}

void TestExcludes::addTestExclude(const char *testName, const GFXCORE_FAMILY family) {
    addExclude(testName, family, pGfxExcludesPerTest);
}

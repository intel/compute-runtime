/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

namespace NEO {
namespace TestExcludes {

bool isTestExcluded(const char *testName, const PRODUCT_FAMILY productFamily, const GFXCORE_FAMILY gfxFamily);
void addTestExclude(const char *testName, const PRODUCT_FAMILY family);
void addTestExclude(const char *testName, const GFXCORE_FAMILY family);
} // namespace TestExcludes
} // namespace NEO

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

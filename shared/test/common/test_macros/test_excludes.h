/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

#include <string>

namespace NEO {
namespace TestExcludes {

bool isTestExcluded(const std::string &testName, const PRODUCT_FAMILY productFamily, const GFXCORE_FAMILY gfxFamily);
void addTestExclude(const char *testName, const PRODUCT_FAMILY family);
void addTestExclude(const char *testName, const GFXCORE_FAMILY family);
} // namespace TestExcludes
} // namespace NEO

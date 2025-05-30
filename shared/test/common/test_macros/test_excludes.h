/*
 * Copyright (C) 2021-2025 Intel Corporation
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

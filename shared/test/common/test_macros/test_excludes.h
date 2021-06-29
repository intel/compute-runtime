/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
namespace TestExcludes {

bool isTestExcluded(const char *testName, const uint32_t product);
void addTestExclude(const char *testName, const uint32_t product);

} // namespace TestExcludes
} // namespace NEO

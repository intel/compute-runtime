/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {
class CompilerProductHelper;

namespace CompilerOptions {

void applyExtraInternalOptions(std::string &internalOptions, const CompilerProductHelper &compilerProductHelper);

} // namespace CompilerOptions
} // namespace NEO

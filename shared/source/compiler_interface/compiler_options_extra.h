/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {
class CompilerProductHelper;

namespace CompilerOptions {
enum class HeaplessMode;

void applyExtraInternalOptions(std::string &internalOptions, const CompilerProductHelper &compilerProductHelper, CompilerOptions::HeaplessMode heaplessMode);

} // namespace CompilerOptions
} // namespace NEO

/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/const_stringref.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {
namespace CompilerOptions {

using TokenizedString = StackVec<ConstStringRef, 32>;
TokenizedString tokenize(ConstStringRef src, char separator = ' ');

} // namespace CompilerOptions
} // namespace NEO
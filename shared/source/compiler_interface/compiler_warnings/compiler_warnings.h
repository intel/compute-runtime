/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

namespace NEO {
namespace CompilerWarnings {

inline constexpr ConstStringRef recompiledFromIr = "warning: module got recompiled from IR because provided native binary is incompatible with underlying device and/or driver [-Wrecompiled-from-ir]";

} // namespace CompilerWarnings
} // namespace NEO

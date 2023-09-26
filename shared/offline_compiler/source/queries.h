/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

namespace NEO {
namespace Queries {
inline constexpr ConstStringRef queryNeoRevision = "NEO_REVISION";
inline constexpr ConstStringRef queryIgcRevision = "IGC_REVISION";
inline constexpr ConstStringRef queryOCLDriverVersion = "OCL_DRIVER_VERSION";
}; // namespace Queries
} // namespace NEO

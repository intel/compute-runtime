/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

namespace NEO {
namespace HeapSize {

inline constexpr size_t defaultHeapSize = 64 * MemoryConstants::kiloByte;

size_t getDefaultHeapSize(size_t defaultValue);

} // namespace HeapSize
} // namespace NEO

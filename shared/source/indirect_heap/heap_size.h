/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/indirect_heap/indirect_heap_type.h"

#include <cstddef>

namespace NEO {
namespace HeapSize {

size_t getDefaultHeapSize(IndirectHeapType heapType);
size_t getHeapSize(size_t defaultValue);

} // namespace HeapSize
} // namespace NEO

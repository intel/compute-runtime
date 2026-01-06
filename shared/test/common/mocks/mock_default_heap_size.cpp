/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/heap_size.h"

namespace NEO {
namespace HeapSize {

size_t getDefaultHeapSize(IndirectHeapType heapType) {
    return MemoryConstants::pageSize64k;
}
} // namespace HeapSize
} // namespace NEO

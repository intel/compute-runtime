/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/heap_size.h"

namespace NEO {
namespace HeapSize {

size_t getDefaultHeapSize(IndirectHeapType heapType) {
    if (heapType == IndirectHeapType::indirectObject && ApiSpecificConfig::getApiType() == ApiSpecificConfig::OCL) {
        return 4 * MemoryConstants::megaByte;
    }
    return MemoryConstants::pageSize64k;
}

} // namespace HeapSize
} // namespace NEO

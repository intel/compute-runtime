/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/heap_size.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {
namespace HeapSize {

size_t getDefaultHeapSize(size_t defaultValue) {
    auto defaultSize = defaultValue;
    if (debugManager.flags.ForceDefaultHeapSize.get() != -1) {
        defaultSize = debugManager.flags.ForceDefaultHeapSize.get() * MemoryConstants::kiloByte;
    }
    return defaultSize;
}

} // namespace HeapSize
} // namespace NEO

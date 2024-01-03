/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/get_extension_function_lookup_map.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"

namespace L0 {
std::unordered_map<std::string, void *> getExtensionFunctionsLookupMap() {
    std::unordered_map<std::string, void *> lookupMap;

#define addToMap(map, x) map[#x] = reinterpret_cast<void *>(&L0::x)
    addToMap(lookupMap, zexDriverImportExternalPointer);
    addToMap(lookupMap, zexDriverReleaseImportedPointer);
    addToMap(lookupMap, zexDriverGetHostPointerBaseAddress);

    addToMap(lookupMap, zexKernelGetBaseAddress);

    addToMap(lookupMap, zexMemGetIpcHandles);
    addToMap(lookupMap, zexMemOpenIpcHandles);

    addToMap(lookupMap, zexCommandListAppendWaitOnMemory);
    addToMap(lookupMap, zexCommandListAppendWaitOnMemory64);
    addToMap(lookupMap, zexCommandListAppendWriteToMemory);
#undef addToMap

    return lookupMap;
}

} // namespace L0

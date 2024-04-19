/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

bool Wddm::isReadOnlyMemory(const void *ptr) {
    if (ptr) {
        MEMORY_BASIC_INFORMATION info;
        SysCalls::virtualQuery(ptr, &info, sizeof(info));
        return info.Protect & PAGE_READONLY;
    }
    return false;
}
} // namespace NEO

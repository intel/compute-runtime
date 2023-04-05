/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/memory/windows/os_memory_imp.h"

namespace L0 {
namespace Sysman {
std::unique_ptr<OsMemory> OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<WddmMemoryImp> pWddmMemoryImp = std::make_unique<WddmMemoryImp>(pOsSysman, onSubdevice, subdeviceId);
    return pWddmMemoryImp;
}

} // namespace Sysman
} // namespace L0

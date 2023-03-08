/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/memory/windows/os_memory_imp.h"

namespace L0 {
namespace Sysman {
OsMemory *OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmMemoryImp *pWddmMemoryImp = new WddmMemoryImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsMemory *>(pWddmMemoryImp);
}

} // namespace Sysman
} // namespace L0

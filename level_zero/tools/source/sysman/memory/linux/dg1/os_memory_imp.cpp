/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/linux/dg1/os_memory_imp.h"

#include "shared/source/os_interface/linux/memory_info_impl.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
}

ze_result_t LinuxMemoryImp::getMemorySize(uint64_t &maxSize, uint64_t &allocSize) {

    if (pDrm->queryMemoryInfo() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto memoryInfo = static_cast<NEO::MemoryInfoImpl *>(pDrm->getMemoryInfo());
    auto region = std::find_if(memoryInfo->regions.begin(), memoryInfo->regions.end(), [](auto tempRegion) {
        return (tempRegion.region.memory_class == I915_MEMORY_CLASS_DEVICE);
    });
    if (region == memoryInfo->regions.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    maxSize = region->probed_size;
    allocSize = maxSize - region->unallocated_size;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getMemHealth(zes_mem_health_t &memHealth) {
    memHealth = ZES_MEM_HEALTH_OK;

    return ZE_RESULT_SUCCESS;
}

OsMemory *OsMemory::create(OsSysman *pOsSysman) {
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman);
    return static_cast<OsMemory *>(pLinuxMemoryImp);
}

} // namespace L0

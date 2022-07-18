/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
}

bool LinuxMemoryImp::isMemoryModuleSupported() {
    return pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->busWidth = -1;
    pProperties->numChannels = -1;
    pProperties->physicalSize = 0;

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxMemoryImp::getState(zes_mem_state_t *pState) {
    std::vector<NEO::MemoryRegion> deviceRegions;
    if (pDrm->queryMemoryInfo() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto memoryInfo = pDrm->getMemoryInfo();
    if (!memoryInfo) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    for (auto region : memoryInfo->getDrmRegionInfos()) {
        if (region.region.memoryClass == drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE) {
            deviceRegions.push_back(region);
        }
    }
    pState->free = deviceRegions[subdeviceId].unallocatedSize;
    pState->size = deviceRegions[subdeviceId].probedSize;
    pState->health = ZES_MEM_HEALTH_OK;

    return ZE_RESULT_SUCCESS;
}

OsMemory *OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsMemory *>(pLinuxMemoryImp);
}

} // namespace L0

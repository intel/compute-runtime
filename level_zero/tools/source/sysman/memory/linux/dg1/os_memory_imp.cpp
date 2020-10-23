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

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
}

bool LinuxMemoryImp::isMemoryModuleSupported() {
    return pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}
static ze_result_t queryMemoryRegions(NEO::Drm *pDrm, drm_i915_memory_region_info &memRegions) {
    if (pDrm->queryMemoryInfo() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto memoryInfo = static_cast<NEO::MemoryInfoImpl *>(pDrm->getMemoryInfo());
    auto regions = std::find_if(memoryInfo->regions.begin(), memoryInfo->regions.end(), [](auto tempRegion) {
        return (tempRegion.region.memory_class == I915_MEMORY_CLASS_DEVICE);
    });
    if (regions == memoryInfo->regions.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    memRegions = *regions;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {

    drm_i915_memory_region_info memRegions = {};
    ze_result_t result = queryMemoryRegions(pDrm, memRegions);

    if (result == ZE_RESULT_SUCCESS) {
        if (memRegions.region.memory_class == I915_MEMORY_CLASS_DEVICE)
            pProperties->location = ZES_MEM_LOC_DEVICE;
    } else {
        // by default failure case also it will be DEVICE
        pProperties->location = ZES_MEM_LOC_DEVICE;
    }

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

    drm_i915_memory_region_info memRegions = {};
    ze_result_t result = queryMemoryRegions(pDrm, memRegions);

    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    pState->health = ZES_MEM_HEALTH_OK;
    pState->free = memRegions.unallocated_size;
    pState->size = memRegions.probed_size;

    return ZE_RESULT_SUCCESS;
}

OsMemory *OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsMemory *>(pLinuxMemoryImp);
}

} // namespace L0

/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDevice = pLinuxSysmanImp->getDeviceHandle();
}
bool LinuxMemoryImp::isMemoryModuleSupported() {
    return pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->location = ZES_MEM_LOC_DEVICE;
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
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsMemory *OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsMemory *>(pLinuxMemoryImp);
}

} // namespace L0

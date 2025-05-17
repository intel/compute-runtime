/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/memory/linux/sysman_os_memory_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getMemoryProperties(pProperties, pLinuxSysmanImp, pDrm, pSysmanKmdInterface, subdeviceId, isSubdevice);
}

ze_result_t LinuxMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getMemoryBandwidth(pBandwidth, pLinuxSysmanImp, subdeviceId);
}

ze_result_t LinuxMemoryImp::getState(zes_mem_state_t *pState) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    pState->health = ZES_MEM_HEALTH_UNKNOWN;
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pSysmanProductHelper->getMemoryHealthIndicator(pFwInterface, &pState->health);

    std::unique_ptr<NEO::MemoryInfo> memoryInfo;
    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    memoryInfo = pDrm->getIoctlHelper()->createMemoryInfo();
    if (!memoryInfo) {
        pState->free = 0;
        pState->size = 0;
        status = ZE_RESULT_ERROR_UNKNOWN;
        if (errno == ENODEV) {
            status = ZE_RESULT_ERROR_DEVICE_LOST;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Error@ %s():createMemoryInfo failed errno:%d \n", __FUNCTION__, errno);
        return status;
    }

    auto region = memoryInfo->getMemoryRegion(MemoryBanks::getBankForLocalMemory(subdeviceId));
    pState->free = region.unallocatedSize;
    pState->size = region.probedSize;
    return status;
}

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getSysmanDeviceImp();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
}

bool LinuxMemoryImp::isMemoryModuleSupported() {
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    return gfxCoreHelper.getEnableLocalMemory(pDevice->getHardwareInfo());
}

std::unique_ptr<OsMemory> OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxMemoryImp> pLinuxMemoryImp = std::make_unique<LinuxMemoryImp>(pOsSysman, onSubdevice, subdeviceId);
    return pLinuxMemoryImp;
}

} // namespace Sysman
} // namespace L0

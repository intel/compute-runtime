/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/engine_info.h"

#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {

std::unique_ptr<OsEngine> OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    if (pLinuxSysmanImp->isUsingPrelimEnabledKmd) {
        return std::make_unique<LinuxEngineImpPrelim>(pOsSysman, type, engineInstance, subDeviceId, onSubDevice);
    }

    return std::make_unique<LinuxEngineImp>(pOsSysman, type, engineInstance, subDeviceId, onSubDevice);
}

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    NEO::Drm *pDrm = &pLinuxSysmanImp->getDrm();

    if (pDrm->sysmanQueryEngineInfo() == false) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():sysmanQueryEngineInfo is returning false and error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto engineInfo = pDrm->getEngineInfo();
    if (pLinuxSysmanImp->isUsingPrelimEnabledKmd) {
        LinuxEngineImpPrelim::getInstancesFromEngineInfo(engineInfo, engineGroupInstance);
    } else {
        LinuxEngineImp::getInstancesFromEngineInfo(engineInfo, engineGroupInstance);
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
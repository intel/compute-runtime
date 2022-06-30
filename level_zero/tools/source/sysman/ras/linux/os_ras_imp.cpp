/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

void OsRas::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {}

ze_result_t LinuxRasImp::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxRasImp::osRasGetConfig(zes_ras_config_t *config) {
    config->totalThreshold = totalThreshold;
    memcpy(config->detailedThresholds.category, categoryThreshold, sizeof(config->detailedThresholds.category));
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxRasImp::osRasSetConfig(const zes_ras_config_t *config) {
    if (pFsAccess->isRootUser() == true) {
        totalThreshold = config->totalThreshold;
        memcpy(categoryThreshold, config->detailedThresholds.category, sizeof(config->detailedThresholds.category));
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
}

ze_result_t LinuxRasImp::osRasGetProperties(zes_ras_properties_t &properties) {
    properties.pNext = nullptr;
    properties.type = osRasErrorType;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}
LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) : osRasErrorType(type), isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
}

OsRas *OsRas::create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman, type, onSubdevice, subdeviceId);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0

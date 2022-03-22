/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp_prelim.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

void OsRas::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {

    constexpr auto maxErrorTypes = 2;
    LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, deviceHandle);
    if (errorType.size() < maxErrorTypes) {
        LinuxRasSourceFabric::getSupportedRasErrorTypes(errorType, pOsSysman, deviceHandle);
        if (errorType.size() < maxErrorTypes) {
            LinuxRasSourceHbm::getSupportedRasErrorTypes(errorType, pOsSysman, deviceHandle);
        }
    }
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

ze_result_t LinuxRasImp::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    if (clear == true) {
        if (pFsAccess->isRootUser() == false) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        }
    }

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    for (auto &rasSource : rasSources) {
        zes_ras_state_t localState = {};
        ze_result_t localResult = rasSource->osRasGetState(localState, clear);
        if (localResult != ZE_RESULT_SUCCESS) {
            continue;
        }
        for (int i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; i++) {
            state.category[i] += localState.category[i];
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

void LinuxRasImp::initSources() {
    rasSources.push_back(std::make_unique<L0::LinuxRasSourceGt>(pLinuxSysmanImp, osRasErrorType, isSubdevice, subdeviceId));
    rasSources.push_back(std::make_unique<L0::LinuxRasSourceFabric>(pLinuxSysmanImp, osRasErrorType, subdeviceId));
    rasSources.push_back(std::make_unique<L0::LinuxRasSourceHbm>(pLinuxSysmanImp, osRasErrorType, subdeviceId));
}

LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) : osRasErrorType(type), isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    initSources();
}

OsRas *OsRas::create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman, type, onSubdevice, subdeviceId);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0

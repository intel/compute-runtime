/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "drm/intel_hwconfig_types.h"

namespace L0 {

static bool isMemoryTypeHbm(LinuxSysmanImp *pLinuxSysmanImp) {
    uint32_t memType = pLinuxSysmanImp->getMemoryType();
    if (memType == INTEL_HWCONFIG_MEMORY_TYPE_HBM2e || memType == INTEL_HWCONFIG_MEMORY_TYPE_HBM2) {
        return true;
    }
    return false;
}

void OsRas::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {

    constexpr auto maxErrorTypes = 2;
    LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, deviceHandle);
    if (errorType.size() < maxErrorTypes) {
        auto pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
        if (isMemoryTypeHbm(pLinuxSysmanImp) == true) {
            LinuxRasSourceHbm::getSupportedRasErrorTypes(errorType, pOsSysman, deviceHandle);
        }
    }
}

ze_result_t LinuxRasImp::osRasGetConfig(zes_ras_config_t *config) {
    config->totalThreshold = totalThreshold;
    memcpy_s(config->detailedThresholds.category, maxRasErrorCategoryCount * sizeof(uint64_t), categoryThreshold, maxRasErrorCategoryCount * sizeof(uint64_t));
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxRasImp::osRasSetConfig(const zes_ras_config_t *config) {
    if (pFsAccess->isRootUser() == true) {
        totalThreshold = config->totalThreshold;
        memcpy_s(categoryThreshold, maxRasErrorCategoryCount * sizeof(uint64_t), config->detailedThresholds.category, maxRasErrorCategoryCount * sizeof(uint64_t));
        return ZE_RESULT_SUCCESS;
    }
    NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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
        for (uint32_t i = 0; i < maxRasErrorCategoryCount; i++) {
            state.category[i] += localState.category[i];
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

void LinuxRasImp::initSources() {
    rasSources.push_back(std::make_unique<L0::LinuxRasSourceGt>(pLinuxSysmanImp, osRasErrorType, isSubdevice, subdeviceId));
    if (isMemoryTypeHbm(pLinuxSysmanImp) == true) {
        rasSources.push_back(std::make_unique<L0::LinuxRasSourceHbm>(pLinuxSysmanImp, osRasErrorType, subdeviceId));
    }
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

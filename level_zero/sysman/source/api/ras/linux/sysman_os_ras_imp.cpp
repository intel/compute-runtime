/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

static bool isMemoryTypeHbm(LinuxSysmanImp *pLinuxSysmanImp) {
    uint32_t memType = pLinuxSysmanImp->getMemoryType();
    if (memType == NEO::DeviceBlobConstants::MemoryType::hbm2e || memType == NEO::DeviceBlobConstants::MemoryType::hbm2) {
        return true;
    }
    return false;
}

void OsRas::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_bool_t isSubDevice, uint32_t subDeviceId) {

    constexpr auto maxErrorTypes = 2;
    LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, isSubDevice, subDeviceId);
    if (errorType.size() < maxErrorTypes) {
        auto pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
        if (isMemoryTypeHbm(pLinuxSysmanImp) == true) {
            LinuxRasSourceHbm::getSupportedRasErrorTypes(errorType, pOsSysman, isSubDevice, subDeviceId);
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
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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

ze_result_t LinuxRasImp::osRasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) {
    ze_result_t result = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    uint32_t totalCategoryCount = 0;
    std::vector<uint32_t> numCategoriesBySources = {};
    for (auto &rasSource : rasSources) {
        totalCategoryCount += rasSource->osRasGetCategoryCount();
        numCategoriesBySources.push_back(totalCategoryCount);
    }

    if (*pCount == 0) {
        *pCount = totalCategoryCount;
        return ZE_RESULT_SUCCESS;
    }

    uint32_t remainingCategories = std::min(totalCategoryCount, *pCount);
    uint32_t numCategoriesAssigned = 0u;
    for (uint32_t rasSourceIdx = 0u; rasSourceIdx < rasSources.size(); rasSourceIdx++) {
        auto &rasSource = rasSources[rasSourceIdx];
        uint32_t numCategoriesRequested = std::min(remainingCategories, numCategoriesBySources[rasSourceIdx]);
        ze_result_t localResult = rasSource->osRasGetStateExp(numCategoriesRequested, &pState[numCategoriesAssigned]);
        if (localResult != ZE_RESULT_SUCCESS) {
            continue;
        }
        remainingCategories -= numCategoriesRequested;
        numCategoriesAssigned += numCategoriesBySources[rasSourceIdx];
        result = localResult;
        if (remainingCategories == 0u) {
            break;
        }
    }
    return result;
}

ze_result_t LinuxRasImp::osRasClearStateExp(zes_ras_error_category_exp_t category) {
    if (pFsAccess->isRootUser() == false) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }

    if (ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS < category) {
        return ZE_RESULT_ERROR_INVALID_ENUMERATION;
    }

    ze_result_t result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (auto &rasSource : rasSources) {
        result = rasSource->osRasClearStateExp(category);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                continue;
            }
            return result;
        }
    }
    return result;
}

void LinuxRasImp::initSources() {
    rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, osRasErrorType, isSubdevice, subdeviceId));
    if (isMemoryTypeHbm(pLinuxSysmanImp) == true) {
        rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, osRasErrorType, isSubdevice, subdeviceId));
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

} // namespace Sysman
} // namespace L0

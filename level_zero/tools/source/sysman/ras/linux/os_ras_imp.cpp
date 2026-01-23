/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include <algorithm>
namespace L0 {

static bool isMemoryTypeHbm(LinuxSysmanImp *pLinuxSysmanImp) {
    uint32_t memType = pLinuxSysmanImp->getMemoryType();
    if (memType == NEO::DeviceBlobConstants::MemoryType::hbm2e || memType == NEO::DeviceBlobConstants::MemoryType::hbm2) {
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
    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
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

ze_result_t LinuxRasImp::osRasGetSupportedCategoriesExp(uint32_t *pCount, zes_ras_error_category_exp_t *pCategories) {

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(supportedErrorCategoriesExp.size());
        return ZE_RESULT_SUCCESS;
    }

    uint32_t numCategoriesToCopy = std::min(*pCount, static_cast<uint32_t>(supportedErrorCategoriesExp.size()));
    for (uint32_t i = 0; i < numCategoriesToCopy; i++) {
        pCategories[i] = supportedErrorCategoriesExp[i];
    }
    *pCount = numCategoriesToCopy;

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxRasImp::osRasGetConfigExp(const uint32_t count, zes_intel_ras_config_exp_t *pConfig) {

    for (uint32_t i = 0; i < count; i++) {
        auto it = std::find_if(categoryExpThresholds.begin(), categoryExpThresholds.end(),
                               [&pConfig, i](const std::pair<zes_ras_error_category_exp_t, uint64_t> &element) {
                                   return element.first == pConfig[i].category;
                               });
        if (it != categoryExpThresholds.end()) {
            pConfig[i].threshold = it->second;
        } else {
            pConfig[i].threshold = 0;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxRasImp::osRasSetConfigExp(const uint32_t count, const zes_intel_ras_config_exp_t *pConfig) {

    if (pFsAccess->isRootUser() == true) {
        categoryExpThresholds.clear();
        for (uint32_t i = 0; i < count; i++) {
            categoryExpThresholds.push_back(std::make_pair(pConfig[i].category, pConfig[i].threshold));
        }
        return ZE_RESULT_SUCCESS;
    }
    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Insufficient permissions and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
    return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
}

ze_result_t LinuxRasImp::osRasGetStateExp(const uint32_t count, zes_intel_ras_state_exp_t *pState) {
    // Fetch states from all RAS sources once
    std::vector<zes_ras_state_exp_t> allSourceStates;

    for (const auto &rasSource : rasSources) {
        uint32_t numCategories = rasSource->osRasGetCategoryCount();
        std::vector<zes_ras_state_exp_t> sourceStates(numCategories);
        ze_result_t result = rasSource->osRasGetStateExp(numCategories, sourceStates.data());
        if (result != ZE_RESULT_SUCCESS) {
            continue;
        }

        // Check for duplicate categories and add to allSourceStates
        for (const auto &state : sourceStates) {
            DEBUG_BREAK_IF(std::find_if(allSourceStates.begin(), allSourceStates.end(),
                                        [&state](const zes_ras_state_exp_t &currentState) { return currentState.category == state.category; }) != allSourceStates.end());
            allSourceStates.push_back(state);
        }
    }

    // Initialize all error counters to 0
    for (uint32_t i = 0; i < count; i++) {
        pState[i].errorCounter = 0;
    }

    // Populate counters directly
    for (const auto &state : allSourceStates) {
        // Find matching category in the requested list
        for (uint32_t i = 0; i < count; i++) {
            if (pState[i].category == state.category) {
                pState[i].errorCounter += state.errorCounter;
                break;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

void LinuxRasImp::initSources() {
    rasSources.push_back(std::make_unique<L0::LinuxRasSourceGt>(pLinuxSysmanImp, osRasErrorType, isSubdevice, subdeviceId));
    if (isMemoryTypeHbm(pLinuxSysmanImp) == true) {
        rasSources.push_back(std::make_unique<L0::LinuxRasSourceHbm>(pLinuxSysmanImp, osRasErrorType, subdeviceId));
    }

    for (const auto &rasSource : rasSources) {
        auto categories = rasSource->getSupportedErrorCategories(osRasErrorType);
        for ([[maybe_unused]] const auto &category : categories) {
            DEBUG_BREAK_IF(std::find(supportedErrorCategoriesExp.begin(), supportedErrorCategoriesExp.end(), category) != supportedErrorCategoriesExp.end());
        }
        supportedErrorCategoriesExp.insert(supportedErrorCategoriesExp.end(), categories.begin(), categories.end());
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

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

void GscRasUtil::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    if (pFwInterface != nullptr) {
        errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
        errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    }
}

ze_result_t GscRasUtil::getMemoryErrorCountFromFw(zes_ras_error_type_t rasErrorType, uint32_t subDeviceCount, uint64_t &errorCount) {
    if (pFwInterface == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return pFwInterface->fwGetMemoryErrorCount(rasErrorType, subDeviceCount, subdeviceId, errorCount);
}

ze_result_t GscRasUtil::rasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    if (clear == true) {
        uint64_t errorCount = 0;
        ze_result_t result = getMemoryErrorCountFromFw(rasErrorType, this->subDeviceCount, errorCount);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        errorBaseline = errorCount; // during clear update the error baseline value
    }

    uint64_t errorCount = 0;
    ze_result_t result = getMemoryErrorCountFromFw(rasErrorType, this->subDeviceCount, errorCount);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS] = errorCount - errorBaseline;
    return ZE_RESULT_SUCCESS;
}

ze_result_t GscRasUtil::rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) {
    uint64_t errorCount = 0;
    ze_result_t result = getMemoryErrorCountFromFw(rasErrorType, this->subDeviceCount, errorCount);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    pState[0].category = ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS;
    pState[0].errorCounter = errorCount - errorBaseline;

    return ZE_RESULT_SUCCESS;
}

ze_result_t GscRasUtil::rasClearStateExp(zes_ras_error_category_exp_t category) {
    if (category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
        uint64_t errorCount = 0;
        ze_result_t result = getMemoryErrorCountFromFw(rasErrorType, this->subDeviceCount, errorCount);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        errorBaseline = errorCount;
    }
    return ZE_RESULT_SUCCESS;
}

uint32_t GscRasUtil::rasGetCategoryCount() {
    // Return one for "MEMORY" category
    return 1u;
}

GscRasUtil::GscRasUtil(zes_ras_error_type_t type, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) : pLinuxSysmanImp(pLinuxSysmanImp), rasErrorType(type), subdeviceId(subdeviceId) {
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
}

} // namespace Sysman
} // namespace L0

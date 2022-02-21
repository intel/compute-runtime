/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp_prelim.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

void LinuxRasSourceHbm::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    if (pFwInterface != nullptr) {
        errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
        errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    }
}

ze_result_t LinuxRasSourceHbm::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    if (pFwInterface == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    uint32_t subDeviceCount = 0;
    pDevice->getSubDevices(&subDeviceCount, nullptr);
    if (clear == true) {
        uint64_t errorCount = 0;
        ze_result_t result = pFwInterface->fwGetMemoryErrorCount(osRasErrorType, subDeviceCount, subdeviceId, errorCount);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        errorBaseline = errorCount; // during clear update the error baseline value
    }
    uint64_t errorCount = 0;
    ze_result_t result = pFwInterface->fwGetMemoryErrorCount(osRasErrorType, subDeviceCount, subdeviceId, errorCount);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS] = errorCount - errorBaseline;
    return ZE_RESULT_SUCCESS;
}

LinuxRasSourceHbm::LinuxRasSourceHbm(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, uint32_t subdeviceId) : pLinuxSysmanImp(pLinuxSysmanImp), osRasErrorType(type), subdeviceId(subdeviceId) {
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
}

} // namespace L0

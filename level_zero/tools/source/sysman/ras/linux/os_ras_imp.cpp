/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

ze_result_t OsRas::getSupportedRasErrorTypes(std::vector<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxRasImp::osRasGetState(zes_ras_state_t &state) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxRasImp::osRasGetProperties(zes_ras_properties_t &properties) {
    properties.pNext = nullptr;
    properties.type = osRasErrorType;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}
LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) : osRasErrorType(type), isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
}

OsRas *OsRas::create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman, type, onSubdevice, subdeviceId);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0

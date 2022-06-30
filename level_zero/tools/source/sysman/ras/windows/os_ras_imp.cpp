/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/ras/os_ras.h"

namespace L0 {

class WddmRasImp : public OsRas {
    ze_result_t osRasGetProperties(zes_ras_properties_t &properties) override;
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetConfig(zes_ras_config_t *config) override;
    ze_result_t osRasSetConfig(const zes_ras_config_t *config) override;
};

void OsRas::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {}

ze_result_t WddmRasImp::osRasGetProperties(zes_ras_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasGetConfig(zes_ras_config_t *config) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasSetConfig(const zes_ras_config_t *config) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsRas *OsRas::create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmRasImp *pWddmRasImp = new WddmRasImp();
    return static_cast<OsRas *>(pWddmRasImp);
}

} // namespace L0

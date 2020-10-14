/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/ras/os_ras.h"

namespace L0 {

class WddmRasImp : public OsRas {
    ze_result_t osRasGetProperties(zes_ras_properties_t &properties) override;
    ze_result_t osRasGetState(zes_ras_state_t &state) override;
};

ze_result_t OsRas::getSupportedRasErrorTypes(std::vector<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasGetProperties(zes_ras_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasGetState(zes_ras_state_t &state) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsRas *OsRas::create(OsSysman *pOsSysman, zes_ras_error_type_t type) {
    WddmRasImp *pWddmRasImp = new WddmRasImp();
    return static_cast<OsRas *>(pWddmRasImp);
}

} // namespace L0

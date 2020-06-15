/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/ras/os_ras.h"

namespace L0 {

class WddmRasImp : public OsRas {
    ze_result_t getCounterValues(zet_ras_details_t *pDetails) override;
    bool isRasSupported(void) override;
    void setRasErrorType(zet_ras_error_type_t type) override;
};

ze_result_t WddmRasImp::getCounterValues(zet_ras_details_t *pDetails) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmRasImp::isRasSupported(void) {
    return false;
}

void WddmRasImp::setRasErrorType(zet_ras_error_type_t type) {}

OsRas *OsRas::create(OsSysman *pOsSysman) {
    WddmRasImp *pWddmRasImp = new WddmRasImp();
    return static_cast<OsRas *>(pWddmRasImp);
}

} // namespace L0

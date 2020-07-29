/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/ras/os_ras.h"

namespace L0 {

class WddmRasImp : public OsRas {
    bool isRasSupported(void) override;
    void setRasErrorType(zes_ras_error_type_t type) override;
};

bool WddmRasImp::isRasSupported(void) {
    return false;
}

void WddmRasImp::setRasErrorType(zes_ras_error_type_t type) {}

OsRas *OsRas::create(OsSysman *pOsSysman) {
    WddmRasImp *pWddmRasImp = new WddmRasImp();
    return static_cast<OsRas *>(pWddmRasImp);
}

} // namespace L0

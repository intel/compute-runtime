/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/power/os_power.h"

namespace L0 {

class WddmPowerImp : public OsPower {};

OsPower *OsPower::create(OsSysman *pOsSysman) {
    WddmPowerImp *pWddmPowerImp = new WddmPowerImp();
    return static_cast<OsPower *>(pWddmPowerImp);
}

} // namespace L0
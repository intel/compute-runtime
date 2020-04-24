/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/temperature/os_temperature.h"

namespace L0 {

class WddmTemperatureImp : public OsTemperature {};

OsTemperature *OsTemperature::create(OsSysman *pOsSysman) {
    WddmTemperatureImp *pWddmTemperatureImp = new WddmTemperatureImp();
    return static_cast<OsTemperature *>(pWddmTemperatureImp);
}

} // namespace L0
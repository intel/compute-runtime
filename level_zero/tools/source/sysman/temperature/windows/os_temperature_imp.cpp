/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/temperature/os_temperature.h"

namespace L0 {

class WddmTemperatureImp : public OsTemperature {
  public:
    ze_result_t getSensorTemperature(double *pTemperature) override;
};

ze_result_t WddmTemperatureImp::getSensorTemperature(double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsTemperature *OsTemperature::create(OsSysman *pOsSysman) {
    WddmTemperatureImp *pWddmTemperatureImp = new WddmTemperatureImp();
    return static_cast<OsTemperature *>(pWddmTemperatureImp);
}

} // namespace L0
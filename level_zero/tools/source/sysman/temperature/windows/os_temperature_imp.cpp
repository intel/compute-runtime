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
    bool isTempModuleSupported() override;
    void setSensorType(zet_temp_sensors_t sensorType) override;
};

ze_result_t WddmTemperatureImp::getSensorTemperature(double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmTemperatureImp::isTempModuleSupported() {
    return false;
}

void WddmTemperatureImp::setSensorType(zet_temp_sensors_t sensorType) {}

OsTemperature *OsTemperature::create(OsSysman *pOsSysman) {
    WddmTemperatureImp *pWddmTemperatureImp = new WddmTemperatureImp();
    return static_cast<OsTemperature *>(pWddmTemperatureImp);
}

} // namespace L0
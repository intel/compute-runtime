/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/sysman_temperature_imp.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t TemperatureImp::temperatureGetProperties(zes_temp_properties_t *pProperties) {
    *pProperties = tempProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t TemperatureImp::temperatureGetConfig(zes_temp_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t TemperatureImp::temperatureSetConfig(const zes_temp_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t TemperatureImp::temperatureGetState(double *pTemperature) {
    return pOsTemperature->getSensorTemperature(pTemperature);
}

void TemperatureImp::init() {
    if (pOsTemperature->isTempModuleSupported()) {
        pOsTemperature->getProperties(&tempProperties);
        this->initSuccess = true;
    }
}

TemperatureImp::TemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t type) {
    pOsTemperature = OsTemperature::create(pOsSysman, onSubdevice, subdeviceId, type);
    init();
}

TemperatureImp::~TemperatureImp() {
}

} // namespace Sysman
} // namespace L0
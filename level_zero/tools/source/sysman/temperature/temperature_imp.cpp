/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/temperature_imp.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

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

TemperatureImp::TemperatureImp(const ze_device_handle_t &deviceHandle, OsSysman *pOsSysman, zes_temp_sensors_t type) {
    uint32_t subdeviceId = 0;
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subdeviceId, onSubdevice);
    pOsTemperature = OsTemperature::create(pOsSysman, onSubdevice,
                                           subdeviceId, type);
    init();
}

TemperatureImp::~TemperatureImp() {
}

} // namespace L0
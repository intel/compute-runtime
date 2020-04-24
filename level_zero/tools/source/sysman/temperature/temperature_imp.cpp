/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/temperature_imp.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t TemperatureImp::temperatureGetProperties(zet_temp_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t TemperatureImp::temperatureGetConfig(zet_temp_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t TemperatureImp::temperatureSetConfig(const zet_temp_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t TemperatureImp::temperatureGetState(double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

TemperatureImp::TemperatureImp(OsSysman *pOsSysman) {
    pOsTemperature = OsTemperature::create(pOsSysman);

    init();
}

TemperatureImp::~TemperatureImp() {
    if (nullptr != pOsTemperature) {
        delete pOsTemperature;
        pOsTemperature = nullptr;
    }
}

} // namespace L0
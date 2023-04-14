/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/temperature/windows/os_temperature_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmTemperatureImp::getProperties(zes_temp_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmTemperatureImp::getSensorTemperature(double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmTemperatureImp::isTempModuleSupported() {
    return false;
}

WddmTemperatureImp::WddmTemperatureImp(OsSysman *pOsSysman) {}

std::unique_ptr<OsTemperature> OsTemperature::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman);
    return std::move(pWddmTemperatureImp);
}

} // namespace Sysman
} // namespace L0

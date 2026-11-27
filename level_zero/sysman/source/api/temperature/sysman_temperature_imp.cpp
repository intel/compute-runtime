/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/sysman_temperature_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {
namespace Sysman {

ze_result_t TemperatureImp::temperatureGetProperties(zes_temp_properties_t *pProperties) {
    const auto stype = pProperties->stype;
    auto pNext = pProperties->pNext;
    *pProperties = tempProperties;
    pProperties->stype = stype;
    pProperties->pNext = pNext;

    auto pExtension = static_cast<zes_base_properties_t *>(pNext);
    while (pExtension != nullptr) {
        if (pExtension->stype == ZES_INTEL_STRUCTURE_TYPE_TEMP_CRITICAL_EXP_PROPERTIES) {
            auto pCriticalProperties = reinterpret_cast<zes_intel_temp_critical_exp_properties_t *>(pExtension);
            if (pOsTemperature->getCriticalTemperature(&pCriticalProperties->criticalTemperature) != ZE_RESULT_SUCCESS) {
                pCriticalProperties->criticalTemperature = 0;
            }
        } else {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not recognize provided extension, stype: 0x%x \n", NEO_FUNCTION_NAME, pExtension->stype);
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        pExtension = static_cast<zes_base_properties_t *>(pExtension->pNext);
    }
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

void TemperatureImp::reInit() {
    pOsTemperature->reInit();
}

void TemperatureImp::init() {
    if (pOsTemperature->isTempModuleSupported()) {
        pOsTemperature->getProperties(&tempProperties);
        this->initSuccess = true;
    }
}

TemperatureImp::TemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t type, uint32_t sensorIndex) {
    pOsTemperature = OsTemperature::create(pOsSysman, onSubdevice, subdeviceId, type, sensorIndex);
    init();
}

TemperatureImp::~TemperatureImp() {
}

} // namespace Sysman
} // namespace L0

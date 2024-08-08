/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/linux/sysman_os_temperature_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxTemperatureImp::getProperties(zes_temp_properties_t *pProperties) {
    pProperties->type = type;
    pProperties->onSubdevice = 0;
    pProperties->subdeviceId = 0;
    if (isSubdevice) {
        pProperties->onSubdevice = isSubdevice;
        pProperties->subdeviceId = subdeviceId;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxTemperatureImp::getGlobalMaxTemperature(double *pTemperature) {
    return pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
}

ze_result_t LinuxTemperatureImp::getGpuMaxTemperature(double *pTemperature) {
    return pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
}

ze_result_t LinuxTemperatureImp::getMemoryMaxTemperature(double *pTemperature) {
    return pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
}

ze_result_t LinuxTemperatureImp::getSensorTemperature(double *pTemperature) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        result = getGlobalMaxTemperature(pTemperature);
        break;
    case ZES_TEMP_SENSORS_GPU:
        result = getGpuMaxTemperature(pTemperature);
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        result = getMemoryMaxTemperature(pTemperature);
        break;
    default:
        *pTemperature = 0;
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    return result;
}

bool LinuxTemperatureImp::isTempModuleSupported() {

    bool result = PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subdeviceId);
    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
    case ZES_TEMP_SENSORS_GPU:
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        result &= pSysmanProductHelper->isMemoryMaxTemperatureSupported();
        break;
    default:
        result = false;
        break;
    }
    return result;
}

void LinuxTemperatureImp::setSensorType(zes_temp_sensors_t sensorType) {
    type = sensorType;
}

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice,
                                         uint32_t subdeviceId) : subdeviceId(subdeviceId), isSubdevice(onSubdevice) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
}

std::unique_ptr<OsTemperature> OsTemperature::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType) {
    std::unique_ptr<LinuxTemperatureImp> pLinuxTemperatureImp = std::make_unique<LinuxTemperatureImp>(pOsSysman, onSubdevice, subdeviceId);
    pLinuxTemperatureImp->setSensorType(sensorType);
    return pLinuxTemperatureImp;
}

} // namespace Sysman
} // namespace L0

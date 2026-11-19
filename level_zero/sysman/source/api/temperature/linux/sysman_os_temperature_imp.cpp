/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/linux/sysman_os_temperature_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxTemperatureImp::getProperties(zes_temp_properties_t *pProperties) {
    pProperties->type = type;
    pProperties->onSubdevice = 0;
    pProperties->subdeviceId = 0;
    pProperties->maxTemperature = defaultMaxTemperature;
    if (isSubdevice) {
        pProperties->onSubdevice = isSubdevice;
        pProperties->subdeviceId = subdeviceId;
    }

    auto result = getMaxTemperature(pProperties->maxTemperature);
    if ((result != ZE_RESULT_SUCCESS) && (result != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get max temperature, leaving maxTemperature at default value %.2f. error:0x%x \n", NEO_FUNCTION_NAME, defaultMaxTemperature, result);
    }

    return ZE_RESULT_SUCCESS;
}

const std::string &LinuxTemperatureImp::getMaxTemperatureFile() const {
    if (type == ZES_TEMP_SENSORS_MEMORY) {
        return memoryMaxTemperatureFile;
    }
    return packageMaxTemperatureFile;
}

ze_result_t LinuxTemperatureImp::getMaxTemperature(double &temperature) {
    const std::string &maxTemperatureFile = getMaxTemperatureFile();
    if (maxTemperatureFile.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    int32_t maxTemperature = 0;
    auto result = pFsAccess->read(maxTemperatureFile, maxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): FsAccess->read() failed to read %s and returning error:0x%x \n", NEO_FUNCTION_NAME, maxTemperatureFile.c_str(), result);
        return result;
    }

    temperature = static_cast<double>(maxTemperature) / milliFactor;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxTemperatureImp::getSensorTemperature(double *pTemperature) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
        break;
    case ZES_TEMP_SENSORS_GPU:
        result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
        break;
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR:
        result = pSysmanProductHelper->getVoltageRegulatorMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
        break;
    case ZES_TEMP_SENSORS_GPU_BOARD:
        result = pSysmanProductHelper->getGpuBoardMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
        break;
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE:
        result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, pTemperature, subdeviceId, sensorIndex);
        break;
    case ZES_TEMP_SENSORS_GPU_BOARD_SINGLE:
        result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, pTemperature, subdeviceId, sensorIndex);
        break;
    case ZES_TEMP_SENSORS_COMPOSITE:
        result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
        break;
    default:
        *pTemperature = 0;
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    return result;
}

bool LinuxTemperatureImp::isHwmonTemperatureFileAvailable(NodeName temperatureNode) {
    return pSysmanProductHelper->isTemperatureReadFromSysfsSupported() &&
           !pLinuxSysmanImp->getIntelGraphicsHwmonFile(subdeviceId, isSubdevice, temperatureNode).empty();
}

bool LinuxTemperatureImp::isTempModuleSupported() {

    bool result = PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subdeviceId);

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR:
    case ZES_TEMP_SENSORS_GPU_BOARD:
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE:
    case ZES_TEMP_SENSORS_GPU_BOARD_SINGLE:
    case ZES_TEMP_SENSORS_COMPOSITE:
        break;
    case ZES_TEMP_SENSORS_GPU:
        result = result || isHwmonTemperatureFileAvailable(NodeName::gpuTemperature);
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        result = (result && pSysmanProductHelper->isMemoryMaxTemperatureSupported()) || isHwmonTemperatureFileAvailable(NodeName::memoryTemperature);
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

void LinuxTemperatureImp::reInit() {
    packageMaxTemperatureFile.clear();
    memoryMaxTemperatureFile.clear();
    init();
}

void LinuxTemperatureImp::init() {
    packageMaxTemperatureFile = pLinuxSysmanImp->getIntelGraphicsHwmonFile(subdeviceId, isSubdevice, NodeName::packageTemperatureEmergency);
    memoryMaxTemperatureFile = pLinuxSysmanImp->getIntelGraphicsHwmonFile(subdeviceId, isSubdevice, NodeName::memoryTemperatureEmergency);
}

void OsTemperature::getSupportedSensors(OsSysman *pOsSysman, std::map<zes_temp_sensors_t, uint32_t> &supportedSensorTypeMap) {
    auto pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pSysmanProductHelper->getSupportedSensors(supportedSensorTypeMap);
}

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice,
                                         uint32_t subdeviceId, uint32_t sensorIndex) : subdeviceId(subdeviceId), sensorIndex(sensorIndex), isSubdevice(onSubdevice) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFsAccess = pLinuxSysmanImp->getSysmanKmdInterface()->getFsAccess();
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    init();
}

std::unique_ptr<OsTemperature> OsTemperature::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType, uint32_t sensorIndex) {
    std::unique_ptr<LinuxTemperatureImp> pLinuxTemperatureImp = std::make_unique<LinuxTemperatureImp>(pOsSysman, onSubdevice, subdeviceId, sensorIndex);
    pLinuxTemperatureImp->setSensorType(sensorType);
    return pLinuxTemperatureImp;
}

} // namespace Sysman
} // namespace L0

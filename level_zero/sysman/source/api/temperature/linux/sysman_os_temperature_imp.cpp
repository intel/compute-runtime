/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/linux/sysman_os_temperature_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

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
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get max temperature, leaving maxTemperature at default value %.2f. error:0x%x \n", __FUNCTION__, defaultMaxTemperature, result);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxTemperatureImp::getMaxTemperature(double &temperature) {
    if (!maxTemperatureFileExists) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    int32_t maxTemperature = 0;
    auto result = pSysfsAccess->read(maxTemperatureFile, maxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, maxTemperatureFile.c_str(), result);
        return result;
    }

    temperature = static_cast<double>(maxTemperature) / milliFactor;
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

ze_result_t LinuxTemperatureImp::getVoltageRegulatorMaxTemperature(double *pTemperature) {
    return pSysmanProductHelper->getVoltageRegulatorMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
}

ze_result_t LinuxTemperatureImp::getGpuBoardMaxTemperature(double *pTemperature) {
    return pSysmanProductHelper->getGpuBoardMaxTemperature(pLinuxSysmanImp, pTemperature, subdeviceId);
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
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR:
        result = getVoltageRegulatorMaxTemperature(pTemperature);
        break;
    case ZES_TEMP_SENSORS_GPU_BOARD:
        result = getGpuBoardMaxTemperature(pTemperature);
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
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR:
    case ZES_TEMP_SENSORS_GPU_BOARD:
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

bool LinuxTemperatureImp::isIntelGraphicsHwmonDir(const std::string &name) {
    return name == pSysmanKmdInterface->getHwmonName(subdeviceId, isSubdevice);
}

void LinuxTemperatureImp::init() {
    const auto maxTemperatureFileName = pSysmanKmdInterface->getTemperatureMaxFileName();
    if (maxTemperatureFileName.empty()) {
        return;
    }
    std::vector<std::string> listOfAllHwmonDirs = {};
    const std::string hwmonDir("device/hwmon");
    if (ZE_RESULT_SUCCESS != pSysfsAccess->scanDirEntries(hwmonDir, listOfAllHwmonDirs)) {
        return;
    }

    for (const auto &tempHwmonDirEntry : listOfAllHwmonDirs) {
        const std::string hwmonNameFile = hwmonDir + "/" + tempHwmonDirEntry + "/name";
        std::string name;
        if (ZE_RESULT_SUCCESS != pSysfsAccess->read(hwmonNameFile, name)) {
            continue;
        }
        if (isIntelGraphicsHwmonDir(name)) {
            intelGraphicsHwmonDir = hwmonDir + "/" + tempHwmonDirEntry;
            break;
        }
    }

    if (intelGraphicsHwmonDir.empty()) {
        return;
    }

    maxTemperatureFile = intelGraphicsHwmonDir + "/" + maxTemperatureFileName;
    maxTemperatureFileExists = pSysfsAccess->fileExists(maxTemperatureFile);
}

void OsTemperature::getSupportedSensors(OsSysman *pOsSysman, std::map<zes_temp_sensors_t, uint32_t> &supportedSensorTypeMap) {
    auto pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pSysmanProductHelper->getSupportedSensors(supportedSensorTypeMap);
}

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice,
                                         uint32_t subdeviceId, uint32_t sensorIndex) : subdeviceId(subdeviceId), isSubdevice(onSubdevice), sensorIndex(sensorIndex) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
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

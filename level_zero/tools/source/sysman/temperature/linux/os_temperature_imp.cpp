/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

constexpr uint32_t numSocTemperatureEntries = 7;     // entries would be PCH, DRAM, SA, PSF, DE, PCIE, TYPEC
constexpr uint32_t numCoreTemperatureEntries = 4;    // entries would be CORE0, CORE1, CORE2, CORE3
constexpr uint32_t numComputeTemperatureEntries = 3; // entries would be IA, GT and LLC
constexpr uint32_t invalidMaxTemperature = 125;
constexpr uint32_t invalidMinTemperature = 10;

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

ze_result_t LinuxTemperatureImp::getGlobalMaxTemperatureNoSubDevice(double *pTemperature) {
    auto isValidTemperature = [](auto temperature) {
        if ((temperature > invalidMaxTemperature) || (temperature < invalidMinTemperature)) {
            return false;
        }
        return true;
    };

    auto getMaxTemperature = [&](auto temperature, auto numTemperatureEntries) {
        uint32_t maxTemperature = 0;
        for (uint32_t count = 0; count < numTemperatureEntries; count++) {
            uint32_t localTemperatureVal = (temperature >> (8 * count)) & 0xff;
            if (isValidTemperature(localTemperatureVal)) {
                if (localTemperatureVal > maxTemperature) {
                    maxTemperature = localTemperatureVal;
                }
            }
        }
        return maxTemperature;
    };

    uint32_t computeTemperature = 0;
    std::string key("COMPUTE_TEMPERATURES");
    ze_result_t result = pPmt->readValue(key, computeTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    // Check max temperature among IA, GT and LLC sensors across COMPUTE_TEMPERATURES
    uint32_t maxComputeTemperature = getMaxTemperature(computeTemperature, numComputeTemperatureEntries);

    uint32_t coreTemperature = 0;
    key = "CORE_TEMPERATURES";
    result = pPmt->readValue(key, coreTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    // Check max temperature among CORE0, CORE1, CORE2, CORE3 sensors across CORE_TEMPERATURES
    uint32_t maxCoreTemperature = getMaxTemperature(coreTemperature, numCoreTemperatureEntries);

    uint64_t socTemperature = 0;
    key = "SOC_TEMPERATURES";
    result = pPmt->readValue(key, socTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    // Check max temperature among PCH, DRAM, SA, PSF, DE, PCIE, TYPEC sensors across SOC_TEMPERATURES
    uint32_t maxSocTemperature = getMaxTemperature(socTemperature, numSocTemperatureEntries);

    *pTemperature = static_cast<double>(std::max({maxComputeTemperature, maxCoreTemperature, maxSocTemperature}));

    return result;
}

ze_result_t LinuxTemperatureImp::getGlobalMaxTemperature(double *pTemperature) {
    if (!isSubdevice) {
        return getGlobalMaxTemperatureNoSubDevice(pTemperature);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxTemperatureImp::getGpuMaxTemperatureNoSubDevice(double *pTemperature) {
    uint32_t computeTemperature = 0;
    std::string key("COMPUTE_TEMPERATURES");
    ze_result_t result = pPmt->readValue(key, computeTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // GT temperature could be read via 8th to 15th bit in the value read in temperature
    computeTemperature = (computeTemperature >> 8) & 0xff;
    *pTemperature = static_cast<double>(computeTemperature);
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxTemperatureImp::getGpuMaxTemperature(double *pTemperature) {
    if (!isSubdevice) {
        return getGpuMaxTemperatureNoSubDevice(pTemperature);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxTemperatureImp::getMemoryMaxTemperature(double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxTemperatureImp::getSensorTemperature(double *pTemperature) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        result = getGlobalMaxTemperature(pTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        break;
    case ZES_TEMP_SENSORS_GPU:
        result = getGpuMaxTemperature(pTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        result = getMemoryMaxTemperature(pTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        break;
    default:
        *pTemperature = 0;
        break;
    }

    return result;
}

bool LinuxTemperatureImp::isTempModuleSupported() {
    if (!isSubdevice) {
        if (type == ZES_TEMP_SENSORS_MEMORY) {
            return false;
        }
    }
    return (pPmt != nullptr);
}

void LinuxTemperatureImp::setSensorType(zes_temp_sensors_t sensorType) {
    type = sensorType;
}

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice,
                                         uint32_t subdeviceId) : subdeviceId(subdeviceId), isSubdevice(onSubdevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pPmt = pLinuxSysmanImp->getPlatformMonitoringTechAccess(subdeviceId);
}

std::unique_ptr<OsTemperature> OsTemperature::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType) {
    std::unique_ptr<LinuxTemperatureImp> pLinuxTemperatureImp = std::make_unique<LinuxTemperatureImp>(pOsSysman, onSubdevice, subdeviceId);
    pLinuxTemperatureImp->setSensorType(sensorType);
    return pLinuxTemperatureImp;
}

} // namespace L0

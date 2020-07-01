/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

#include "level_zero/tools/source/sysman/linux/pmt.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {
constexpr uint64_t numSocTemperatureEntries = 7;
constexpr uint32_t numCoreTemperatureEntries = 4;

ze_result_t LinuxTemperatureImp::getSensorTemperature(double *pTemperature) {
    uint64_t socTemperature = 0;
    uint32_t computeTemperature = 0, coreTemperature = 0;
    std::string key("COMPUTE_TEMPERATURES");
    ze_result_t result = pPmt->readValue(key, computeTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // GT temperature could be read via 8th to 15th bit in the value read in temperature
    computeTemperature = (computeTemperature >> 8) & 0xff;

    switch (type) {
    case ZET_TEMP_SENSORS_GPU:
        *pTemperature = static_cast<double>(computeTemperature);
        break;
    case ZET_TEMP_SENSORS_GLOBAL:
        key = "SOC_TEMPERATURES";
        result = pPmt->readValue(key, socTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }

        uint64_t socTemperatureList[numSocTemperatureEntries];
        for (uint64_t count = 0; count < numSocTemperatureEntries; count++) {
            socTemperatureList[count] = (socTemperature >> (8 * count)) & 0xff;
        }
        // Assign socTemperature so that it contains the maximum temperature provided by all SOC components
        socTemperature = *std::max_element(socTemperatureList, socTemperatureList + numSocTemperatureEntries);

        key = "CORE_TEMPERATURES";
        result = pPmt->readValue(key, coreTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }

        uint32_t coreTemperatureList[numCoreTemperatureEntries];
        for (uint64_t count = 0; count < numCoreTemperatureEntries; count++) {
            coreTemperatureList[count] = (coreTemperature >> (8 * count)) & 0xff;
        }
        // Assign coreTemperature so that it contains the maximum temperature provided by all SOC components
        coreTemperature = *std::max_element(coreTemperatureList, coreTemperatureList + numCoreTemperatureEntries);

        *pTemperature = static_cast<double>(std::max({static_cast<uint64_t>(computeTemperature),
                                                      static_cast<uint64_t>(coreTemperature), socTemperature}));
        break;
    default:
        *pTemperature = 0;
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    return result;
}

bool LinuxTemperatureImp::isTempModuleSupported() {
    return pPmt->isPmtSupported();
}

void LinuxTemperatureImp::setSensorType(zet_temp_sensors_t sensorType) {
    type = sensorType;
}

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pPmt = &pLinuxSysmanImp->getPlatformMonitoringTechAccess();
}

OsTemperature *OsTemperature::create(OsSysman *pOsSysman) {
    LinuxTemperatureImp *pLinuxTemperatureImp = new LinuxTemperatureImp(pOsSysman);
    return static_cast<OsTemperature *>(pLinuxTemperatureImp);
}

} // namespace L0

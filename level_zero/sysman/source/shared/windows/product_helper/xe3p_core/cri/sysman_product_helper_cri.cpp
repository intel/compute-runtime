/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.inl"

#include <algorithm>
#include <bit>

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_CRI;

#include "level_zero/sysman/source/shared/product_helper/sysman_os_agnostic_product_helper_xe2_and_later.inl"

constexpr static uint32_t maxVrTemperatureSensorCount = 4;
constexpr static uint32_t maxGpuBoardTemperatureSensorCount = 2;
constexpr static uint32_t temperatureNotAvailable = 0xFFFFFFFF;

static std::map<unsigned long, std::map<std::string, uint32_t>> guidToKeyOffsetMap = {
    {0x1e2fa030, // CRI PUNIT rev 0
     {{"AMB_TEMPERATURE", 22},
      {"COMPOSITE_TEMPERATURE", 68},
      {"VR_TEMPERATURE_0", 56},
      {"VR_TEMPERATURE_1", 57},
      {"VR_TEMPERATURE_2", 58},
      {"VR_TEMPERATURE_3", 59}}},
    {0x5e2fa230, // CRI OOBMSM Rev 0
     {{"SOC_TOPDIE_TEMPERATURE", 32},
      {"VRAM_TEMPERATURE", 33}}},
    {0x5e2fa270, {}} // CRI GFSP Rev 0
};

template <>
std::map<unsigned long, std::map<std::string, uint32_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template <>
uint32_t SysmanProductHelperHw<gfxProduct>::getPmtMaxInterfaces() {
    return 3u; // PUNIT, OOBMSM, GFSP
}

template <>
void SysmanProductHelperHw<gfxProduct>::getSupportedSensors(std::map<zes_temp_sensors_t, uint32_t> &supportedSensorTypeMap) {
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GLOBAL] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GPU] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_MEMORY] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_VOLTAGE_REGULATOR] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GPU_BOARD] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE] = maxVrTemperatureSensorCount;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GPU_BOARD_SINGLE] = maxGpuBoardTemperatureSensorCount;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_COMPOSITE] = 1;
}

static ze_result_t readVoltageRegulatorTemperature(PlatformMonitoringTech *pPmt, double *pTemperature, uint32_t sensorIndex) {
    std::string key = "VR_TEMPERATURE_" + std::to_string(sensorIndex);

    uint32_t rawVrTemperature = 0;
    ze_result_t result = pPmt->readValue(key, rawVrTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read VR temperature value for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), result);
        return result;
    }

    if (rawVrTemperature == temperatureNotAvailable) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): VR temperature is not available for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    *pTemperature = static_cast<double>(std::bit_cast<float>(rawVrTemperature));
    return ZE_RESULT_SUCCESS;
}

static ze_result_t readVoltageRegulatorMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    ze_result_t result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    bool isAnySensorAvailable = false;
    double vrMaxTemperature = 0;
    for (uint32_t sensorIndex = 0; sensorIndex < maxVrTemperatureSensorCount; sensorIndex++) {
        double vrTemperature = 0;
        result = readVoltageRegulatorTemperature(pPmt, &vrTemperature, sensorIndex);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stdout, "Info@ %s(): VR temperature sensor %u is not available, ignoring it for the max VR temperature, error:0x%x \n", NEO_FUNCTION_NAME, sensorIndex, result);
            continue;
        }
        vrMaxTemperature = isAnySensorAvailable ? std::max(vrMaxTemperature, vrTemperature) : vrTemperature;
        isAnySensorAvailable = true;
    }

    if (!isAnySensorAvailable) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): None of the VR temperature sensors are available, returning error:0x%x \n", NEO_FUNCTION_NAME, result);
        return result;
    }

    *pTemperature = vrMaxTemperature;
    return ZE_RESULT_SUCCESS;
}

static ze_result_t readGpuBoardTemperature(PlatformMonitoringTech *pPmt, double *pTemperature, uint32_t sensorIndex) {
    std::string key = "AMB_TEMPERATURE";

    uint64_t ambientTemperatureContainer = 0;
    ze_result_t result = pPmt->readValue(key, ambientTemperatureContainer);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read ambient temperature value for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), result);
        return result;
    }

    uint32_t rawAmbientTemperature = static_cast<uint32_t>((ambientTemperatureContainer >> (32 * sensorIndex)) & 0xFFFFFFFF);

    if (rawAmbientTemperature == temperatureNotAvailable) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Ambient temperature is not available for sensor index %u of key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, sensorIndex, key.c_str(), ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    *pTemperature = static_cast<double>(std::bit_cast<float>(rawAmbientTemperature));
    return ZE_RESULT_SUCCESS;
}

static ze_result_t readGpuBoardMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    ze_result_t result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    bool isAnySensorAvailable = false;
    double gpuBoardMaxTemperature = 0;
    for (uint32_t sensorIndex = 0; sensorIndex < maxGpuBoardTemperatureSensorCount; sensorIndex++) {
        double gpuBoardTemperature = 0;
        result = readGpuBoardTemperature(pPmt, &gpuBoardTemperature, sensorIndex);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stdout, "Info@ %s(): GPU board temperature sensor %u is not available, ignoring it for the max GPU board temperature, error:0x%x \n", NEO_FUNCTION_NAME, sensorIndex, result);
            continue;
        }
        gpuBoardMaxTemperature = isAnySensorAvailable ? std::max(gpuBoardMaxTemperature, gpuBoardTemperature) : gpuBoardTemperature;
        isAnySensorAvailable = true;
    }

    if (!isAnySensorAvailable) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): None of the GPU board temperature sensors are available, returning error:0x%x \n", NEO_FUNCTION_NAME, result);
        return result;
    }

    *pTemperature = gpuBoardMaxTemperature;
    return ZE_RESULT_SUCCESS;
}

static ze_result_t readGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t rawGpuMaxTemperature = 0;
    std::string key("SOC_TOPDIE_TEMPERATURE");
    ze_result_t result = pPmt->readValue(key, rawGpuMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read value for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), result);
        return result;
    }

    if (rawGpuMaxTemperature == temperatureNotAvailable) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): GPU temperature is not available for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    *pTemperature = static_cast<double>(std::bit_cast<float>(rawGpuMaxTemperature));

    return ZE_RESULT_SUCCESS;
}

static ze_result_t readMemoryMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t memoryMaxTemperature = 0;
    std::string key("VRAM_TEMPERATURE");
    ze_result_t result = pPmt->readValue(key, memoryMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read value for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), result);
        return result;
    }
    memoryMaxTemperature &= 0xFFu; // Extract least significant 8 bits
    *pTemperature = static_cast<double>(memoryMaxTemperature);

    return ZE_RESULT_SUCCESS;
}

static ze_result_t readCompositeTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t compositeTemperature = 0;
    std::string key("COMPOSITE_TEMPERATURE");
    ze_result_t result = pPmt->readValue(key, compositeTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read value for key: %s, returning error:0x%x \n", NEO_FUNCTION_NAME, key.c_str(), result);
        return result;
    }

    if (compositeTemperature == temperatureNotAvailable) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Composite temperature is not reported by the hardware, returning error:0x%x \n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    *pTemperature = static_cast<double>(std::bit_cast<float>(compositeTemperature));

    return ZE_RESULT_SUCCESS;
}

static ze_result_t readGlobalMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    double gpuMaxTemperature = 0;
    ze_result_t result = readGpuMaxTemperature(pPmt, &gpuMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    double memoryMaxTemperature = 0;
    result = readMemoryMaxTemperature(pPmt, &memoryMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    double vrMaxTemperature = 0;
    result = readVoltageRegulatorMaxTemperature(pPmt, &vrMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    double gpuBoardMaxTemperature = 0;
    result = readGpuBoardMaxTemperature(pPmt, &gpuBoardMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *pTemperature = std::max({gpuMaxTemperature, memoryMaxTemperature, vrMaxTemperature, gpuBoardMaxTemperature});
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, uint32_t sensorIndex, WddmSysmanImp *pWddmSysmanImp) {
    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    if (pPmt == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): PMT interface is not available, returning error:0x%x \n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        result = readGlobalMaxTemperature(pPmt, pTemperature);
        break;
    case ZES_TEMP_SENSORS_GPU:
        result = readGpuMaxTemperature(pPmt, pTemperature);
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        result = readMemoryMaxTemperature(pPmt, pTemperature);
        break;
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR:
        result = readVoltageRegulatorMaxTemperature(pPmt, pTemperature);
        break;
    case ZES_TEMP_SENSORS_GPU_BOARD:
        result = readGpuBoardMaxTemperature(pPmt, pTemperature);
        break;
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE:
        result = readVoltageRegulatorTemperature(pPmt, pTemperature, sensorIndex);
        break;
    case ZES_TEMP_SENSORS_GPU_BOARD_SINGLE:
        result = readGpuBoardTemperature(pPmt, pTemperature, sensorIndex);
        break;
    case ZES_TEMP_SENSORS_COMPOSITE:
        result = readCompositeTemperature(pPmt, pTemperature);
        break;
    default:
        *pTemperature = 0;
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    return result;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isTempModuleSupported(zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) {
    bool result = (pWddmSysmanImp->getSysmanPmt() != nullptr);

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
    case ZES_TEMP_SENSORS_GPU:
    case ZES_TEMP_SENSORS_MEMORY:
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR:
    case ZES_TEMP_SENSORS_GPU_BOARD:
    case ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE:
    case ZES_TEMP_SENSORS_GPU_BOARD_SINGLE:
    case ZES_TEMP_SENSORS_COMPOSITE:
        break;
    default:
        result = false;
        break;
    }
    return result;
}

template class SysmanProductHelperHw<gfxProduct>;

static EnableSysmanProductHelper<gfxProduct> enableCri;

} // namespace Sysman
} // namespace L0

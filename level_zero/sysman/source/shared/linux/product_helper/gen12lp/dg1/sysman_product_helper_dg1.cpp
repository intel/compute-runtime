/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_DG1;

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    auto isValidTemperature = [](auto temperature) {
        if ((temperature > invalidMaxTemperature) || (temperature < invalidMinTemperature)) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): temperature:%f is not in valid limits \n", __FUNCTION__, temperature);
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

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    uint32_t maxComputeTemperature = 0;
    uint32_t maxCoreTemperature = 0;
    std::string key;

    uint32_t computeTemperature = 0;
    key = "COMPUTE_TEMPERATURES";
    result = pPmt->readValue(key, computeTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for COMPUTE_TEMPERATURES is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    // Check max temperature among IA, GT and LLC sensors across COMPUTE_TEMPERATURES
    maxComputeTemperature = getMaxTemperature(computeTemperature, numComputeTemperatureEntries);

    uint32_t coreTemperature = 0;
    key = "CORE_TEMPERATURES";
    result = pPmt->readValue(key, coreTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for CORE_TEMPERATURES is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    // Check max temperature among CORE0, CORE1, CORE2, CORE3 sensors across CORE_TEMPERATURES
    maxCoreTemperature = getMaxTemperature(coreTemperature, numCoreTemperatureEntries);

    uint64_t socTemperature = 0;
    key = "SOC_TEMPERATURES";
    result = pPmt->readValue(key, socTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for SOC_TEMPERATURES is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    // Check max temperature among possible sensors like PCH or GT_TEMP, DRAM, SA, PSF, DE, PCIE, TYPEC across SOC_TEMPERATURES
    uint32_t maxSocTemperature = getMaxTemperature(socTemperature, numSocTemperatureEntries);

    *pTemperature = static_cast<double>(std::max({maxComputeTemperature, maxCoreTemperature, maxSocTemperature}));

    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    double gpuMaxTemperature = 0;

    // In DG1 platform, Gpu Max Temperature is obtained from COMPUTE_TEMPERATURE only
    uint32_t computeTemperature = 0;
    std::string key("COMPUTE_TEMPERATURES");
    ze_result_t result = pPmt->readValue(key, computeTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for COMPUTE_TEMPERATURES is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    // GT temperature could be read via 8th to 15th bit in the value read in temperature
    computeTemperature = (computeTemperature >> 8) & 0xff;
    gpuMaxTemperature = static_cast<double>(computeTemperature);

    *pTemperature = gpuMaxTemperature;
    return ZE_RESULT_SUCCESS;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

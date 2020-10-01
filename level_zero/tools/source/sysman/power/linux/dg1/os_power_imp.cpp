/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/dg1/os_power_imp.h"

#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxPowerImp::hwmonDir("device/hwmon");
const std::string LinuxPowerImp::i915("i915");
const std::string LinuxPowerImp::sustainedPowerLimitEnabled("power1_max_enable");
const std::string LinuxPowerImp::sustainedPowerLimit("power1_max");
const std::string LinuxPowerImp::sustainedPowerLimitInterval("power1_max_interval");
const std::string LinuxPowerImp::burstPowerLimitEnabled("power1_cap_enable");
const std::string LinuxPowerImp::burstPowerLimit("power1_cap");
const std::string LinuxPowerImp::energyCounterNode("energy1_input");

void powerGetTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t LinuxPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    auto result = pSysfsAccess->read(i915HwmonDir + "/" + energyCounterNode, pEnergy->energy);
    if (ZE_RESULT_SUCCESS != result) {
        return getErrorCode(result);
    }
    powerGetTimestamp(pEnergy->timestamp);
    return result;
}

ze_result_t LinuxPowerImp::getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint64_t val = 0;
    if (pSustained != nullptr) {
        result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimitEnabled, val);
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }
        pSustained->enabled = static_cast<ze_bool_t>(val);

        if (pSustained->enabled) {
            val = 0;
            result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
            val /= milliFactor; // Convert microWatts to milliwatts
            pSustained->power = static_cast<int32_t>(val);

            val = 0;
            result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimitInterval, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
            pSustained->interval = static_cast<int32_t>(val);
        }
    }
    if (pBurst != nullptr) {
        result = pSysfsAccess->read(i915HwmonDir + "/" + burstPowerLimitEnabled, val);
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }
        pBurst->enabled = static_cast<ze_bool_t>(val);

        if (pBurst->enabled) {
            result = pSysfsAccess->read(i915HwmonDir + "/" + burstPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
            val /= milliFactor; // Convert microWatts to milliwatts
            pBurst->power = static_cast<int32_t>(val);
        }
    }
    if (pPeak != nullptr) {
        pPeak->powerAC = -1;
        pPeak->powerDC = -1;
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

ze_result_t LinuxPowerImp::setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    int32_t val = 0;
    if (pSustained != nullptr) {
        uint64_t isSustainedPowerLimitEnabled = 0;
        result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimitEnabled, isSustainedPowerLimitEnabled);
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }

        if (isSustainedPowerLimitEnabled) {
            val = static_cast<uint32_t>(pSustained->power) * milliFactor; // Convert milliWatts to microwatts
            result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }

            result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimitInterval, pSustained->interval);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
        }
    }
    if (pBurst != nullptr) {
        result = pSysfsAccess->write(i915HwmonDir + "/" + burstPowerLimitEnabled, static_cast<int>(pBurst->enabled));
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }

        if (pBurst->enabled) {
            val = static_cast<uint32_t>(pBurst->power) * milliFactor; // Convert milliWatts to microwatts
            result = pSysfsAccess->write(i915HwmonDir + "/" + burstPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
        }
    }

    return result;
}

ze_result_t LinuxPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxPowerImp::isPowerModuleSupported() {
    std::vector<std::string> listOfAllHwmonDirs = {};
    if (ZE_RESULT_SUCCESS != pSysfsAccess->scanDirEntries(hwmonDir, listOfAllHwmonDirs)) {
        return false;
    }
    for (const auto &tempHwmonDirEntry : listOfAllHwmonDirs) {
        const std::string i915NameFile = hwmonDir + "/" + tempHwmonDirEntry + "/" + "name";
        std::string name;
        if (ZE_RESULT_SUCCESS != pSysfsAccess->read(i915NameFile, name)) {
            continue;
        }
        if (name == i915) {
            i915HwmonDir = hwmonDir + "/" + tempHwmonDirEntry;
            return true;
        }
    }

    return false;
}

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0
/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"
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
const std::string LinuxPowerImp::defaultPowerLimit("power_default_limit");
const std::string LinuxPowerImp::minPowerLimit("power_min_limit");
const std::string LinuxPowerImp::maxPowerLimit("power_max_limit");

void powerGetTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t LinuxPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->canControl = canControl;
    pProperties->isEnergyThresholdSupported = false;
    pProperties->defaultLimit = -1;
    pProperties->minLimit = -1;
    pProperties->maxLimit = -1;

    uint32_t val = 0;
    auto result = pSysfsAccess->read(i915HwmonDir + "/" + defaultPowerLimit, val);
    if (ZE_RESULT_SUCCESS == result) {
        pProperties->defaultLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
    }

    result = pSysfsAccess->read(i915HwmonDir + "/" + minPowerLimit, val);
    if (ZE_RESULT_SUCCESS == result && val != 0) {
        pProperties->minLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
    }

    result = pSysfsAccess->read(i915HwmonDir + "/" + maxPowerLimit, val);
    if (ZE_RESULT_SUCCESS == result && val != std::numeric_limits<uint32_t>::max()) {
        pProperties->maxLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getPmtEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    const std::string key("PACKAGE_ENERGY");
    uint64_t energy = 0;
    constexpr uint64_t fixedPointToJoule = 1048576;
    ze_result_t result = pPmt->readValue(key, energy);
    // PMT will return energy counter in Q20 format(fixed point representation) where first 20 bits(from LSB) represent decimal part and remaining integral part which is converted into joule by division with 1048576(2^20) and then converted into microjoules
    pEnergy->energy = (energy / fixedPointToJoule) * convertJouleToMicroJoule;
    return result;
}
ze_result_t LinuxPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    powerGetTimestamp(pEnergy->timestamp);
    ze_result_t result = pSysfsAccess->read(i915HwmonDir + "/" + energyCounterNode, pEnergy->energy);
    if (result != ZE_RESULT_SUCCESS) {
        if (pPmt != nullptr) {
            return getPmtEnergyCounter(pEnergy);
        }
    }
    if (result != ZE_RESULT_SUCCESS) {
        return getErrorCode(result);
    }
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
        if (isSustainedPowerLimitEnabled != static_cast<uint64_t>(pSustained->enabled)) {
            result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimitEnabled, static_cast<int>(pSustained->enabled));
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
            isSustainedPowerLimitEnabled = static_cast<uint64_t>(pSustained->enabled);
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
        result = ZE_RESULT_SUCCESS;
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

ze_result_t LinuxPowerImp::getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxPowerImp::isHwmonDir(std::string name) {
    if (isSubdevice == false && (name == i915)) {
        return true;
    }
    return false;
}

bool LinuxPowerImp::isPowerModuleSupported() {
    std::vector<std::string> listOfAllHwmonDirs = {};
    bool hwmonDirExists = false;
    if (ZE_RESULT_SUCCESS != pSysfsAccess->scanDirEntries(hwmonDir, listOfAllHwmonDirs)) {
        hwmonDirExists = false;
    }
    for (const auto &tempHwmonDirEntry : listOfAllHwmonDirs) {
        const std::string i915NameFile = hwmonDir + "/" + tempHwmonDirEntry + "/" + "name";
        std::string name;
        if (ZE_RESULT_SUCCESS != pSysfsAccess->read(i915NameFile, name)) {
            continue;
        }
        if (isHwmonDir(name)) {
            i915HwmonDir = hwmonDir + "/" + tempHwmonDirEntry;
            hwmonDirExists = true;
            canControl = true;
        }
    }
    if (hwmonDirExists == false) {
        return (pPmt != nullptr);
    }
    return true;
}

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pPmt = pLinuxSysmanImp->getPlatformMonitoringTechAccess(subdeviceId);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp_prelim.h"

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxPowerImp::hwmonDir("device/hwmon");
const std::string LinuxPowerImp::i915("i915");
const std::string LinuxPowerImp::sustainedPowerLimit("power1_max");
const std::string LinuxPowerImp::criticalPowerLimit("power1_crit");
const std::string LinuxPowerImp::energyCounterNode("energy1_input");
const std::string LinuxPowerImp::defaultPowerLimit("power1_max_default");

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
    if (!isSubdevice) {
        uint32_t val = 0;
        auto result = pSysfsAccess->read(i915HwmonDir + "/" + defaultPowerLimit, val);
        if (ZE_RESULT_SUCCESS == result) {
            pProperties->defaultLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
        }
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
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    if (!isSubdevice) {
        uint64_t val = 0;
        if (pSustained != nullptr) {
            val = 0;
            result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
            val /= milliFactor; // Convert microWatts to milliwatts
            pSustained->power = static_cast<int32_t>(val);
            pSustained->enabled = true;
            pSustained->interval = -1;
        }
        if (pBurst != nullptr) {
            pBurst->power = -1;
            pBurst->enabled = false;
        }
        if (pPeak != nullptr) {
            result = pSysfsAccess->read(i915HwmonDir + "/" + criticalPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
            val /= milliFactor; // Convert microWatts to milliwatts
            pPeak->powerAC = static_cast<int32_t>(val);
            pPeak->powerDC = -1;
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

ze_result_t LinuxPowerImp::setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    if (!isSubdevice) {
        int32_t val = 0;
        if (pSustained != nullptr) {
            val = static_cast<uint32_t>(pSustained->power) * milliFactor; // Convert milliWatts to microwatts
            result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
        }
        if (pPeak != nullptr) {
            val = static_cast<uint32_t>(pPeak->powerAC) * milliFactor; // Convert milliWatts to microwatts
            result = pSysfsAccess->write(i915HwmonDir + "/" + criticalPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

ze_result_t LinuxPowerImp::getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

// The top-level hwmon(hwmon1 in example) contains all the power related information and device level
// energy counters. The other hwmon directories contain per tile energy counters.
// ex:- device/hwmon/hwmon1/energy1_input                name = "i915"   (Top level hwmon)
//      device/hwmon/hwmon2/energy1_input                name = "i915_gt0"  (Tile 0)
//      device/hwmon/hwmon3/energy1_input                name = "i915_gt1"  (Tile 1)

bool LinuxPowerImp::isHwmonDir(std::string name) {
    if (isSubdevice == true) {
        if (name == (i915 + "_gt" + std::to_string(subdeviceId))) {
            return true;
        }
    } else if (name == i915) {
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
            canControl = isSubdevice ? false : true;
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

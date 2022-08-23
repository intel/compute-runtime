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
const std::string LinuxPowerImp::sustainedPowerLimitInterval("power1_max_interval");
const std::string LinuxPowerImp::energyCounterNode("energy1_input");
const std::string LinuxPowerImp::defaultPowerLimit("power1_rated_max");

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
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getPropertiesExt(zes_power_ext_properties_t *pExtPoperties) {
    pExtPoperties->domain = isSubdevice ? ZES_POWER_DOMAIN_PACKAGE : ZES_POWER_DOMAIN_CARD;
    if (pExtPoperties->defaultLimit) {
        if (!isSubdevice) {
            uint32_t val = 0;
            ze_result_t result = pSysfsAccess->read(i915HwmonDir + "/" + defaultPowerLimit, val);
            if (result == ZE_RESULT_SUCCESS) {
                pExtPoperties->defaultLimit->limit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
            } else {
                return getErrorCode(result);
            }
        } else {
            pExtPoperties->defaultLimit->limit = -1;
        }
        pExtPoperties->defaultLimit->limitUnit = ZES_LIMIT_UNIT_POWER;
        pExtPoperties->defaultLimit->enabledStateLocked = true;
        pExtPoperties->defaultLimit->intervalValueLocked = true;
        pExtPoperties->defaultLimit->limitValueLocked = true;
        pExtPoperties->defaultLimit->source = ZES_POWER_SOURCE_ANY;
        pExtPoperties->defaultLimit->level = ZES_POWER_LEVEL_UNKNOWN;
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
            val /= milliFactor; // Convert microwatts to milliwatts
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
            val /= milliFactor; // Convert microwatts to milliwatts
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
            val = static_cast<uint32_t>(pSustained->power) * milliFactor; // Convert milliwatts to microwatts
            result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }
        }
        if (pPeak != nullptr) {
            val = static_cast<uint32_t>(pPeak->powerAC) * milliFactor; // Convert milliwatts to microwatts
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
    ze_result_t result = ZE_RESULT_SUCCESS;
    if ((*pCount == 0) || (powerLimitCount < *pCount)) {
        *pCount = powerLimitCount;
    }

    if (pSustained != nullptr) {
        uint64_t val = 0;
        uint8_t count = 0;
        if (count < *pCount) {
            result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }

            int32_t interval = 0;
            result = pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimitInterval, interval);
            if (ZE_RESULT_SUCCESS != result) {
                return getErrorCode(result);
            }

            val /= milliFactor; // Convert microwatts to milliwatts
            pSustained[count].limit = static_cast<int32_t>(val);
            pSustained[count].enabledStateLocked = true;
            pSustained[count].intervalValueLocked = false;
            pSustained[count].limitValueLocked = false;
            pSustained[count].source = ZES_POWER_SOURCE_ANY;
            pSustained[count].level = ZES_POWER_LEVEL_SUSTAINED;
            pSustained[count].limitUnit = ZES_LIMIT_UNIT_POWER;
            pSustained[count].interval = interval;
            count++;
        }

        if (count < *pCount) {
            result = pSysfsAccess->read(i915HwmonDir + "/" + criticalPowerLimit, val);
            if (result != ZE_RESULT_SUCCESS) {
                return getErrorCode(result);
            }
            pSustained[count].enabledStateLocked = true;
            pSustained[count].intervalValueLocked = true;
            pSustained[count].limitValueLocked = false;
            pSustained[count].source = ZES_POWER_SOURCE_ANY;
            pSustained[count].level = ZES_POWER_LEVEL_PEAK;
            pSustained[count].interval = 0; // Hardcode to 100 micro seconds i.e 0.1 milli seconds
            if ((productFamily == IGFX_PVC) || (productFamily == IGFX_XE_HP_SDV)) {
                pSustained[count].limit = static_cast<int32_t>(val);
                pSustained[count].limitUnit = ZES_LIMIT_UNIT_CURRENT;
            } else {
                val /= milliFactor; // Convert microwatts to milliwatts
                pSustained[count].limit = static_cast<int32_t>(val);
                pSustained[count].limitUnit = ZES_LIMIT_UNIT_POWER;
            }
        }
    }
    return result;
}

ze_result_t LinuxPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    if (!isSubdevice) {
        uint64_t val = 0;
        for (uint32_t i = 0; i < *pCount; i++) {
            if (pSustained[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                val = pSustained[i].limit * milliFactor; // Convert milliwatts to microwatts
                result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimit, val);
                if (ZE_RESULT_SUCCESS != result) {
                    return getErrorCode(result);
                }

                result = pSysfsAccess->write(i915HwmonDir + "/" + sustainedPowerLimitInterval, pSustained[i].interval);
                if (ZE_RESULT_SUCCESS != result) {
                    return getErrorCode(result);
                }
            } else if (pSustained[i].level == ZES_POWER_LEVEL_PEAK) {
                if ((productFamily == IGFX_PVC) || (productFamily == IGFX_XE_HP_SDV)) {
                    val = pSustained[i].limit;
                } else {
                    val = pSustained[i].limit * milliFactor; // Convert milliwatts to microwatts
                }
                result = pSysfsAccess->write(i915HwmonDir + "/" + criticalPowerLimit, val);
                if (ZE_RESULT_SUCCESS != result) {
                    return getErrorCode(result);
                }
            } else {
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
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

    if (!isSubdevice) {
        uint64_t val = 0;
        if (ZE_RESULT_SUCCESS == pSysfsAccess->read(i915HwmonDir + "/" + sustainedPowerLimit, val)) {
            powerLimitCount++;
        }

        if (ZE_RESULT_SUCCESS == pSysfsAccess->read(i915HwmonDir + "/" + criticalPowerLimit, val)) {
            powerLimitCount++;
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
    productFamily = SysmanDeviceImp::getProductFamily(pLinuxSysmanImp->getDeviceHandle());
    if ((productFamily == IGFX_PVC) || (productFamily == IGFX_XE_HP_SDV)) {
        criticalPowerLimit = "curr1_crit";
    } else {
        criticalPowerLimit = "power1_crit";
    }
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0

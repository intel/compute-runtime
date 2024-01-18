/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

class LinuxPowerImp::PowerLimitRestorer : NEO::NonCopyableOrMovableClass {
  public:
    PowerLimitRestorer(L0::Sysman::SysFsAccessInterface *pSysfsAccess, std::string powerLimit) : pSysfsAccess(pSysfsAccess), powerLimit(powerLimit) {
        result = pSysfsAccess->read(powerLimit, powerLimitValue);
    }

    ~PowerLimitRestorer() {
        if (result == ZE_RESULT_SUCCESS) {
            result = pSysfsAccess->write(powerLimit, powerLimitValue);
            DEBUG_BREAK_IF(result != ZE_RESULT_SUCCESS);
        }
    }
    operator ze_result_t() const {
        return result;
    }

  protected:
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    std::string powerLimit = {};
    uint64_t powerLimitValue = 0;
};

ze_result_t LinuxPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->canControl = canControl;
    pProperties->isEnergyThresholdSupported = false;
    pProperties->defaultLimit = -1;
    pProperties->minLimit = -1;
    pProperties->maxLimit = -1;

    if (isSubdevice) {
        return ZE_RESULT_SUCCESS;
    }

    auto result = getDefaultLimit(pProperties->defaultLimit);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto powerLimitRestorer = L0::Sysman::LinuxPowerImp::PowerLimitRestorer(pSysfsAccess, sustainedPowerLimit);
    if (powerLimitRestorer != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimit.c_str(), getErrorCode(powerLimitRestorer));
        return getErrorCode(powerLimitRestorer);
    }

    result = getMinLimit(pProperties->minLimit);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    return getMaxLimit(pProperties->maxLimit);
}

ze_result_t LinuxPowerImp::getMinLimit(int32_t &minLimit) {
    // Fw clamps to minimum value if power limit requested to set is less than min limit, Set to 100 micro watt to get min limit
    uint64_t powerLimit = 100;
    auto result = pSysfsAccess->write(sustainedPowerLimit, powerLimit);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimit.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    result = pSysfsAccess->read(sustainedPowerLimit, powerLimit);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimit.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), powerLimit, powerLimit);
    minLimit = static_cast<int32_t>(powerLimit);

    return result;
}

ze_result_t LinuxPowerImp::getMaxLimit(int32_t &maxLimit) {
    // Fw clamps to maximum value if power limit requested to set is greater than max limit, Set to max value to get max limit
    uint64_t powerLimit = std::numeric_limits<int32_t>::max();
    auto result = pSysfsAccess->write(sustainedPowerLimit, powerLimit);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimit.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    result = pSysfsAccess->read(sustainedPowerLimit, powerLimit);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimit.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), powerLimit, powerLimit);
    maxLimit = static_cast<int32_t>(powerLimit);

    return result;
}

ze_result_t LinuxPowerImp::getDefaultLimit(int32_t &defaultLimit) {
    uint64_t powerLimit = 0;
    std::string defaultPowerLimit = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameDefaultPowerLimit, subdeviceId, false);
    auto result = pSysfsAccess->read(defaultPowerLimit, powerLimit);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), defaultPowerLimit.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameDefaultPowerLimit), powerLimit, powerLimit);
    defaultLimit = static_cast<int32_t>(powerLimit);

    return result;
}

ze_result_t LinuxPowerImp::getPropertiesExt(zes_power_ext_properties_t *pExtPoperties) {
    pExtPoperties->domain = isSubdevice ? ZES_POWER_DOMAIN_PACKAGE : ZES_POWER_DOMAIN_CARD;
    if (pExtPoperties->defaultLimit) {
        if (!isSubdevice) {
            uint64_t val = 0;
            std::string defaultPowerLimit = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameDefaultPowerLimit, subdeviceId, false);
            ze_result_t result = pSysfsAccess->read(defaultPowerLimit, val);
            if (result == ZE_RESULT_SUCCESS) {
                pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameDefaultPowerLimit), val, val);
                pExtPoperties->defaultLimit->limit = static_cast<int32_t>(val);
            } else {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), defaultPowerLimit.c_str(), getErrorCode(result));
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
    pEnergy->timestamp = SysmanDevice::getSysmanTimestamp();
    std::string energyCounterNode = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEnergyCounterNode, subdeviceId, false);
    ze_result_t result = pSysfsAccess->read(energyCounterNode, pEnergy->energy);
    if (result != ZE_RESULT_SUCCESS) {
        if (pPmt != nullptr) {
            return getPmtEnergyCounter(pEnergy);
        }
    }
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), energyCounterNode.c_str(), getErrorCode(result));
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
            result = pSysfsAccess->read(sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), val, val);
            pSustained->power = static_cast<int32_t>(val);
            pSustained->enabled = true;
            pSustained->interval = -1;
        }
        if (pBurst != nullptr) {
            pBurst->power = -1;
            pBurst->enabled = false;
        }
        if (pPeak != nullptr) {
            result = pSysfsAccess->read(criticalPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit), val, val);
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
        uint64_t val = 0;

        if (pSustained != nullptr) {
            val = static_cast<uint64_t>(pSustained->power);
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), SysmanKmdInterface::milli, val, val);
            result = pSysfsAccess->write(sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
        }
        if (pPeak != nullptr) {
            val = static_cast<uint64_t>(pPeak->powerAC);
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit), SysmanKmdInterface::milli, val, val);
            result = pSysfsAccess->write(criticalPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}
ze_result_t LinuxPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
            result = pSysfsAccess->read(sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

            int32_t interval = 0;
            result = pSysfsAccess->read(sustainedPowerLimitInterval, interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitInterval.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

            pSysmanKmdInterface->convertSysfsValueUnit(SysmanKmdInterface::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), val, val);
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
            result = pSysfsAccess->read(criticalPowerLimit, val);
            if (result != ZE_RESULT_SUCCESS) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            pSustained[count].enabledStateLocked = true;
            pSustained[count].intervalValueLocked = true;
            pSustained[count].limitValueLocked = false;
            pSustained[count].source = ZES_POWER_SOURCE_ANY;
            pSustained[count].level = ZES_POWER_LEVEL_PEAK;
            pSustained[count].interval = 0;
            pSustained[count].limit = pSysmanProductHelper->getPowerLimitValue(val);
            pSustained[count].limitUnit = pSysmanProductHelper->getPowerLimitUnit();
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
                val = static_cast<uint64_t>(pSustained[i].limit);
                pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), SysmanKmdInterface::milli, val, val);
                result = pSysfsAccess->write(sustainedPowerLimit, val);
                if (ZE_RESULT_SUCCESS != result) {
                    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimit.c_str(), getErrorCode(result));
                    return getErrorCode(result);
                }

                result = pSysfsAccess->write(sustainedPowerLimitInterval, pSustained[i].interval);
                if (ZE_RESULT_SUCCESS != result) {
                    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitInterval.c_str(), getErrorCode(result));
                    return getErrorCode(result);
                }
            } else if (pSustained[i].level == ZES_POWER_LEVEL_PEAK) {
                val = pSysmanProductHelper->setPowerLimitValue(pSustained[i].limit);
                result = pSysfsAccess->write(criticalPowerLimit, val);
                if (ZE_RESULT_SUCCESS != result) {
                    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimit.c_str(), getErrorCode(result));
                    return getErrorCode(result);
                }
            } else {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
        }
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

bool LinuxPowerImp::isIntelGraphicsHwmonDir(const std::string &name) {
    std::string intelGraphicsHwmonName = pSysmanKmdInterface->getHwmonName(subdeviceId, isSubdevice);
    if (name == intelGraphicsHwmonName) {
        return true;
    }
    return false;
}

bool LinuxPowerImp::isPowerModuleSupported() {
    std::vector<std::string> listOfAllHwmonDirs = {};
    bool hwmonDirExists = false;
    const std::string hwmonDir("device/hwmon");
    if (ZE_RESULT_SUCCESS != pSysfsAccess->scanDirEntries(hwmonDir, listOfAllHwmonDirs)) {
        hwmonDirExists = false;
    }
    for (const auto &tempHwmonDirEntry : listOfAllHwmonDirs) {
        const std::string hwmonNameFile = hwmonDir + "/" + tempHwmonDirEntry + "/" + "name";
        std::string name;
        if (ZE_RESULT_SUCCESS != pSysfsAccess->read(hwmonNameFile, name)) {
            continue;
        }
        if (isIntelGraphicsHwmonDir(name)) {
            intelGraphicsHwmonDir = hwmonDir + "/" + tempHwmonDirEntry;
            hwmonDirExists = true;
            canControl = isSubdevice ? false : true;
        }
    }

    if (!isSubdevice) {
        uint64_t val = 0;
        sustainedPowerLimit = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimit, subdeviceId, false);
        criticalPowerLimit = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCriticalPowerLimit, subdeviceId, false);
        sustainedPowerLimitInterval = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimitInterval, subdeviceId, false);
        if (ZE_RESULT_SUCCESS == pSysfsAccess->read(sustainedPowerLimit, val)) {
            powerLimitCount++;
        }

        if (ZE_RESULT_SUCCESS == pSysfsAccess->read(criticalPowerLimit, val)) {
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
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsPower *>(pLinuxPowerImp);
}
} // namespace Sysman
} // namespace L0

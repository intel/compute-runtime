/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

static const std::string hwmonDir("device/hwmon");

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

    pProperties->maxLimit = pProperties->defaultLimit;

    return result;
}

ze_result_t LinuxPowerImp::getDefaultLimit(int32_t &defaultLimit) {
    std::string defaultPowerLimitFile = {};
    if (powerDomain == ZES_POWER_DOMAIN_CARD) {
        defaultPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNameDefaultPowerLimit);
    } else if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        defaultPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNamePackageDefaultPowerLimit);
    }

    uint64_t powerLimit = 0;
    auto result = pSysfsAccess->read(defaultPowerLimitFile, powerLimit);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), defaultPowerLimitFile.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameDefaultPowerLimit), powerLimit, powerLimit);
    defaultLimit = static_cast<int32_t>(powerLimit);

    return result;
}

ze_result_t LinuxPowerImp::getPropertiesExt(zes_power_ext_properties_t *pExtPoperties) {
    pExtPoperties->domain = powerDomain;
    if (pExtPoperties->defaultLimit) {
        pExtPoperties->defaultLimit->limit = -1;
        pExtPoperties->defaultLimit->limitUnit = ZES_LIMIT_UNIT_POWER;
        pExtPoperties->defaultLimit->enabledStateLocked = true;
        pExtPoperties->defaultLimit->intervalValueLocked = true;
        pExtPoperties->defaultLimit->limitValueLocked = true;
        pExtPoperties->defaultLimit->source = ZES_POWER_SOURCE_ANY;
        pExtPoperties->defaultLimit->level = ZES_POWER_LEVEL_UNKNOWN;
        if (!isSubdevice) {
            auto result = getDefaultLimit(pExtPoperties->defaultLimit->limit);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getPmtEnergyCounter(zes_power_energy_counter_t *pEnergy) {

    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    if (!PlatformMonitoringTech::getKeyOffsetMap(pSysmanProductHelper, guid, keyOffsetMap)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    const std::string key("PACKAGE_ENERGY");
    uint64_t energy = 0;
    constexpr uint64_t fixedPointToJoule = 1048576;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, energy)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // PMT will return energy counter in Q20 format(fixed point representation) where first 20 bits(from LSB) represent decimal part and
    // remaining integral part which is converted into joule by division with 1048576(2^20) and then converted into microjoules
    pEnergy->energy = (energy / fixedPointToJoule) * convertJouleToMicroJoule;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    // Try to get energy counter from PMT if supported
    bool isTelemetrySupportAvailable = PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subdeviceId);
    if (isTelemetrySupportAvailable && (pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(powerDomain))) {
        result = getPmtEnergyCounter(pEnergy);
    }

    // If PMT method fails, try to read from sysfs
    if (result != ZE_RESULT_SUCCESS) {
        result = pSysfsAccess->read(energyCounterNodeFile, pEnergy->energy);
    }

    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), energyCounterNodeFile.c_str(), getErrorCode(result));
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    pEnergy->timestamp = SysmanDevice::getSysmanTimestamp();

    return result;
}

ze_result_t LinuxPowerImp::getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    if (isSubdevice) {
        return result;
    }

    uint64_t val = 0;

    if (pSustained != nullptr) {
        val = 0;
        result = pSysfsAccess->read(sustainedPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), val, val);
        pSustained->power = static_cast<int32_t>(val);
        pSustained->enabled = true;
        pSustained->interval = -1;
    }

    if (pBurst != nullptr) {
        pBurst->power = -1;
        pBurst->enabled = false;
    }

    if (pPeak != nullptr) {
        result = pSysfsAccess->read(criticalPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit), val, val);
        pPeak->powerAC = static_cast<int32_t>(val);
        pPeak->powerDC = -1;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    if (!canControl) {
        return result;
    }

    uint64_t val = 0;

    if (pSustained != nullptr) {
        val = static_cast<uint64_t>(pSustained->power);
        pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), SysfsValueUnit::milli, val, val);
        result = pSysfsAccess->write(sustainedPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
    }

    if (pPeak != nullptr) {
        val = static_cast<uint64_t>(pPeak->powerAC);
        pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit), SysfsValueUnit::milli, val, val);
        result = pSysfsAccess->write(criticalPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pLimitExt) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    if ((*pCount == 0) || (powerLimitCount < *pCount)) {
        *pCount = powerLimitCount;
    }

    if (isSubdevice || pLimitExt == nullptr) {
        return result;
    }

    uint64_t val = 0;
    uint8_t count = 0;

    if (count < *pCount) {
        result = pSysfsAccess->read(sustainedPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }

        int32_t interval = 0;
        result = pSysfsAccess->read(sustainedPowerLimitIntervalFile, interval);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitIntervalFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }

        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), val, val);
        pLimitExt[count].limit = static_cast<int32_t>(val);
        pLimitExt[count].enabledStateLocked = true;
        pLimitExt[count].intervalValueLocked = false;
        pLimitExt[count].limitValueLocked = false;
        pLimitExt[count].source = ZES_POWER_SOURCE_ANY;
        pLimitExt[count].level = ZES_POWER_LEVEL_SUSTAINED;
        pLimitExt[count].limitUnit = ZES_LIMIT_UNIT_POWER;
        pLimitExt[count].interval = interval;
        count++;
    }

    if (count < *pCount) {
        result = pSysfsAccess->read(criticalPowerLimitFile, val);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        pLimitExt[count].enabledStateLocked = true;
        pLimitExt[count].intervalValueLocked = true;
        pLimitExt[count].limitValueLocked = false;
        pLimitExt[count].source = ZES_POWER_SOURCE_ANY;
        pLimitExt[count].level = ZES_POWER_LEVEL_PEAK;
        pLimitExt[count].interval = 0;
        pLimitExt[count].limit = pSysmanProductHelper->getPowerLimitValue(val);
        pLimitExt[count].limitUnit = pSysmanProductHelper->getPowerLimitUnit();
    }

    return result;
}

ze_result_t LinuxPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    if (!canControl) {
        return result;
    }

    uint64_t val = 0;
    for (uint32_t i = 0; i < *pCount; i++) {
        if (pSustained[i].level == ZES_POWER_LEVEL_SUSTAINED) {
            val = static_cast<uint64_t>(pSustained[i].limit);
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit), SysfsValueUnit::milli, val, val);
            result = pSysfsAccess->write(sustainedPowerLimitFile, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

            result = pSysfsAccess->write(sustainedPowerLimitIntervalFile, pSustained[i].interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitIntervalFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

        } else if (pSustained[i].level == ZES_POWER_LEVEL_PEAK) {
            val = static_cast<uint64_t>(pSustained[i].limit);
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit), SysfsValueUnit::milli, val, val);
            result = pSysfsAccess->write(criticalPowerLimitFile, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    return ZE_RESULT_SUCCESS;
}

bool LinuxPowerImp::isIntelGraphicsHwmonDir(const std::string &name) {
    std::string intelGraphicsHwmonName = pSysmanKmdInterface->getHwmonName(subdeviceId, isSubdevice);
    return (name == intelGraphicsHwmonName);
}

bool LinuxPowerImp::isPowerModuleSupported() {

    if (isSubdevice && !(pSysmanKmdInterface->isPowerSupportForSubdeviceAvailable(powerDomain))) {
        return false;
    }

    if (!pSysfsAccess->fileExists(energyCounterNodeFile) && !pSysfsAccess->fileExists(sustainedPowerLimitFile) && !pSysfsAccess->fileExists(criticalPowerLimitFile)) {
        return false;
    }

    return true;
}

void LinuxPowerImp::getPowerLimitFiles() {
    std::vector<std::string> listOfAllHwmonDirs = {};
    if (ZE_RESULT_SUCCESS != pSysfsAccess->scanDirEntries(hwmonDir, listOfAllHwmonDirs)) {
        return;
    }

    for (const auto &tempHwmonDirEntry : listOfAllHwmonDirs) {
        const std::string hwmonNameFile = hwmonDir + "/" + tempHwmonDirEntry + "/" + "name";
        std::string name;
        if (ZE_RESULT_SUCCESS != pSysfsAccess->read(hwmonNameFile, name)) {
            continue;
        }
        if (isIntelGraphicsHwmonDir(name)) {
            intelGraphicsHwmonDir = hwmonDir + "/" + tempHwmonDirEntry;
            canControl = (!isSubdevice) && (pSysmanProductHelper->isPowerSetLimitSupported());
            break;
        }
    }

    if (intelGraphicsHwmonDir.empty()) {
        return;
    }

    std::string fileName = pSysmanKmdInterface->getEnergyCounterNodeFilePath(isSubdevice, powerDomain);
    if (fileName.empty()) {
        return;
    }
    energyCounterNodeFile = intelGraphicsHwmonDir + "/" + fileName;

    if (isSubdevice) {
        return;
    }

    if (powerDomain == ZES_POWER_DOMAIN_CARD) {
        sustainedPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNameSustainedPowerLimit);
        criticalPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNameCriticalPowerLimit);
        sustainedPowerLimitIntervalFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNameSustainedPowerLimitInterval);
    } else if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        sustainedPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNamePackageSustainedPowerLimit);
        criticalPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNamePackageCriticalPowerLimit);
        sustainedPowerLimitIntervalFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePathForPower(SysfsName::sysfsNamePackageSustainedPowerLimitInterval);
    } else {
        return;
    }

    if (pSysfsAccess->fileExists(sustainedPowerLimitFile)) {
        powerLimitCount++;
    }

    if (pSysfsAccess->fileExists(criticalPowerLimitFile)) {
        powerLimitCount++;
    }
}

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) : isSubdevice(onSubdevice), subdeviceId(subdeviceId), powerDomain(powerDomain) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    getPowerLimitFiles();
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, powerDomain);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

std::vector<zes_power_domain_t> OsPower::getPowerDomains(OsSysman *pOsSysman) {
    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};
    return powerDomains;
}

} // namespace Sysman
} // namespace L0

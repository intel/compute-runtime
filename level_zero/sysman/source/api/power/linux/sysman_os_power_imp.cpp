/*
 * Copyright (C) 2023-2025 Intel Corporation
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

    [[maybe_unused]] auto result = getDefaultLimit(pProperties->defaultLimit);

    pProperties->maxLimit = pProperties->defaultLimit;

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getDefaultLimit(int32_t &defaultLimit) {
    std::string defaultPowerLimitFile = {};
    if (powerDomain == ZES_POWER_DOMAIN_CARD) {
        defaultPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardDefaultPowerLimit, subdeviceId, false);
    } else if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        defaultPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageDefaultPowerLimit, subdeviceId, false);
    } else {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint64_t powerLimit = 0;
    auto result = pSysfsAccess->read(defaultPowerLimitFile, powerLimit);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), defaultPowerLimitFile.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }

    pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageDefaultPowerLimit), powerLimit, powerLimit);
    defaultLimit = static_cast<int32_t>(powerLimit);

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getPropertiesExt(zes_power_ext_properties_t *pExtProperties) {
    pExtProperties->domain = powerDomain;
    if (pExtProperties->defaultLimit) {
        pExtProperties->defaultLimit->limit = -1;
        pExtProperties->defaultLimit->limitUnit = ZES_LIMIT_UNIT_POWER;
        pExtProperties->defaultLimit->enabledStateLocked = true;
        pExtProperties->defaultLimit->intervalValueLocked = true;
        pExtProperties->defaultLimit->limitValueLocked = true;
        pExtProperties->defaultLimit->source = ZES_POWER_SOURCE_ANY;
        pExtProperties->defaultLimit->level = ZES_POWER_LEVEL_UNKNOWN;
        if (!isSubdevice) {
            [[maybe_unused]] auto result = getDefaultLimit(pExtProperties->defaultLimit->limit);
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    if (isTelemetrySupportAvailable) {
        if ((result = pSysmanProductHelper->getPowerEnergyCounter(pEnergy, pLinuxSysmanImp, powerDomain, subdeviceId)) == ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    if ((result = pSysfsAccess->read(energyCounterNodeFile, pEnergy->energy)) != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), energyCounterNodeFile.c_str(), getErrorCode(result));
        return result;
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
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit), val, val);
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
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageCriticalPowerLimit), val, val);
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
        pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit), SysfsValueUnit::milli, val, val);
        result = pSysfsAccess->write(sustainedPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    if (pPeak != nullptr) {
        val = static_cast<uint64_t>(pPeak->powerAC);
        pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageCriticalPowerLimit), SysfsValueUnit::milli, val, val);
        result = pSysfsAccess->write(criticalPowerLimitFile, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), criticalPowerLimitFile.c_str(), getErrorCode(result));
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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

    bool sustainedLimitReadSuccess = true;
    bool sustainedLimitIntervalReadSuccess = true;
    bool burstLimitReadSuccess = true;
    bool burstLimitIntervalReadSuccess = true;

    uint64_t powerLimit = 0;
    int32_t interval = 0;
    uint8_t count = 0;

    if (sustainedPowerLimitFileExists) {
        result = pSysfsAccess->read(sustainedPowerLimitFile, powerLimit);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
            sustainedLimitReadSuccess = false;
        }

        if (sustainedLimitReadSuccess) {
            result = pSysfsAccess->read(sustainedPowerLimitIntervalFile, interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitIntervalFile.c_str(), getErrorCode(result));
                sustainedLimitIntervalReadSuccess = false;
            }
        }

        if (!sustainedLimitReadSuccess) {
            return getErrorCode(result);
        }

        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit), powerLimit, powerLimit);
        pLimitExt[count].limit = static_cast<int32_t>(powerLimit);
        pLimitExt[count].enabledStateLocked = true;
        pLimitExt[count].intervalValueLocked = false;
        pLimitExt[count].limitValueLocked = false;
        pLimitExt[count].source = ZES_POWER_SOURCE_ANY;
        pLimitExt[count].level = ZES_POWER_LEVEL_SUSTAINED;
        pLimitExt[count].limitUnit = ZES_LIMIT_UNIT_POWER;
        pLimitExt[count].interval = sustainedLimitIntervalReadSuccess ? interval : -1;
        count++;
    }

    if (burstPowerLimitFileExists) {
        powerLimit = 0;
        result = pSysfsAccess->read(burstPowerLimitFile, powerLimit);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimitFile.c_str(), getErrorCode(result));
            burstLimitReadSuccess = false;
        }

        if (burstLimitReadSuccess) {
            interval = 0;
            result = pSysfsAccess->read(burstPowerLimitIntervalFile, interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimitIntervalFile.c_str(), getErrorCode(result));
                burstLimitIntervalReadSuccess = false;
            }
        }

        if (!burstLimitReadSuccess) {
            return getErrorCode(result);
        }

        pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit), powerLimit, powerLimit);
        pLimitExt[count].limit = static_cast<int32_t>(powerLimit);
        pLimitExt[count].enabledStateLocked = true;
        pLimitExt[count].intervalValueLocked = false;
        pLimitExt[count].limitValueLocked = false;
        pLimitExt[count].source = ZES_POWER_SOURCE_ANY;
        pLimitExt[count].level = ZES_POWER_LEVEL_BURST;
        pLimitExt[count].limitUnit = ZES_LIMIT_UNIT_POWER;
        pLimitExt[count].interval = burstLimitIntervalReadSuccess ? interval : -1;
        count++;
    }

    if (criticalPowerLimitFileExists) {
        powerLimit = 0;
        result = pSysfsAccess->read(criticalPowerLimitFile, powerLimit);
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
        pLimitExt[count].limit = pSysmanProductHelper->getPowerLimitValue(powerLimit);
        pLimitExt[count].limitUnit = pSysmanProductHelper->getPowerLimitUnit();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pLimitExt) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    if (!canControl) {
        return result;
    }

    uint64_t val = 0;

    for (uint32_t i = 0; i < *pCount; i++) {
        if (pLimitExt[i].level == ZES_POWER_LEVEL_SUSTAINED) {
            val = static_cast<uint64_t>(pLimitExt[i].limit);
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit), SysfsValueUnit::milli, val, val);
            result = pSysfsAccess->write(sustainedPowerLimitFile, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

            result = pSysfsAccess->write(sustainedPowerLimitIntervalFile, pLimitExt[i].interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitIntervalFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
        } else if (pLimitExt[i].level == ZES_POWER_LEVEL_BURST) {
            val = pSysmanProductHelper->setPowerLimitValue(pLimitExt[i].limit);
            result = pSysfsAccess->write(burstPowerLimitFile, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

            result = pSysfsAccess->write(burstPowerLimitIntervalFile, pLimitExt[i].interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimitIntervalFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
        } else if (pLimitExt[i].level == ZES_POWER_LEVEL_PEAK) {
            val = pSysmanProductHelper->setPowerLimitValue(pLimitExt[i].limit);
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
    if (name == intelGraphicsHwmonName) {
        return true;
    }
    return false;
}

void LinuxPowerImp::init() {
    std::vector<std::string> listOfAllHwmonDirs = {};
    const std::string hwmonDir("device/hwmon");
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

    std::string fileName = pSysmanKmdInterface->getEnergyCounterNodeFile(powerDomain);
    if (!fileName.empty()) {
        energyCounterNodeFile = intelGraphicsHwmonDir + "/" + fileName;
    }

    if (isSubdevice) {
        return;
    }

    if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        burstPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageBurstPowerLimit, subdeviceId, false);
        burstPowerLimitIntervalFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageBurstPowerLimitInterval, subdeviceId, false);
        criticalPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageCriticalPowerLimit, subdeviceId, false);
        sustainedPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimit, subdeviceId, false);
        sustainedPowerLimitIntervalFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimitInterval, subdeviceId, false);
    } else if (powerDomain == ZES_POWER_DOMAIN_CARD) {
        burstPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardBurstPowerLimit, subdeviceId, false);
        burstPowerLimitIntervalFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardBurstPowerLimitInterval, subdeviceId, false);
        criticalPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardCriticalPowerLimit, subdeviceId, false);
        sustainedPowerLimitFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardSustainedPowerLimit, subdeviceId, false);
        sustainedPowerLimitIntervalFile = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardSustainedPowerLimitInterval, subdeviceId, false);
    } else {
        return;
    }

    if (pSysfsAccess->fileExists(burstPowerLimitFile)) {
        powerLimitCount++;
        burstPowerLimitFileExists = true;
    }

    if (pSysfsAccess->fileExists(criticalPowerLimitFile)) {
        powerLimitCount++;
        criticalPowerLimitFileExists = true;
    }

    if (pSysfsAccess->fileExists(sustainedPowerLimitFile)) {
        powerLimitCount++;
        sustainedPowerLimitFileExists = true;
    }
}

bool LinuxPowerImp::isPowerModuleSupported() {
    bool isEnergyCounterAvailable = (pSysfsAccess->fileExists(energyCounterNodeFile) || isTelemetrySupportAvailable);

    if (isSubdevice) {
        return isEnergyCounterAvailable;
    }

    return isEnergyCounterAvailable || sustainedPowerLimitFileExists || burstPowerLimitFileExists || criticalPowerLimitFileExists;
}

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) : isSubdevice(onSubdevice), subdeviceId(subdeviceId), powerDomain(powerDomain) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    isTelemetrySupportAvailable = PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subdeviceId);
    init();
}

std::vector<zes_power_domain_t> OsPower::getSupportedPowerDomains(OsSysman *pOsSysman) {
    auto pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    std::vector<zes_power_domain_t> powerDomains = pSysmanKmdInterface->getPowerDomains();
    return powerDomains;
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, powerDomain);
    return static_cast<OsPower *>(pLinuxPowerImp);
}
} // namespace Sysman
} // namespace L0

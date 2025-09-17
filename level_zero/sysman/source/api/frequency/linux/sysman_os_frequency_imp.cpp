/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/frequency/linux/sysman_os_frequency_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include "neo_igfxfmid.h"

#include <cmath>

namespace L0 {
namespace Sysman {

ze_result_t LinuxFrequencyImp::osFrequencyGetProperties(zes_freq_properties_t &properties) {
    properties.pNext = nullptr;
    properties.canControl = canControl;
    properties.type = frequencyDomainNumber;
    ze_result_t result1 = getMinVal(properties.min);
    ze_result_t result2 = getMaxVal(properties.max);
    // If can't figure out the valid range, then can't control it.
    if (ZE_RESULT_SUCCESS != result1 || ZE_RESULT_SUCCESS != result2) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getMinVal returned: 0x%x, getMaxVal returned: 0x%x> <setting min = 0.0, max = 0.0>\n", __func__, result1, result2);
        properties.canControl = false;
        properties.min = 0.0;
        properties.max = 0.0;
    }
    properties.isThrottleEventSupported = false;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}

double LinuxFrequencyImp::osFrequencyGetStepSize() {
    double stepSize;
    pSysmanProductHelper->getFrequencyStepSize(&stepSize);
    return stepSize;
}

ze_result_t LinuxFrequencyImp::osFrequencyGetRange(zes_freq_range_t *pLimits) {
    ze_result_t result = getMax(pLimits->max);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getMax returned 0x%x setting max = -1>\n", __func__, result);
        pLimits->max = -1;
    }

    result = getMin(pLimits->min);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getMin returned 0x%x setting min = -1>\n", __func__, result);
        pLimits->min = -1;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::osFrequencySetRange(const zes_freq_range_t *pLimits) {
    double newMin = round(pLimits->min);
    double newMax = round(pLimits->max);

    if (!canControl) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (pSysmanKmdInterface->isDefaultFrequencyAvailable()) {
        if (newMax == -1 && newMin == -1) {
            double maxDefault = 0, minDefault = 0;
            ze_result_t result1, result2, result;
            result1 = pSysfsAccess->read(maxDefaultFreqFile, maxDefault);
            result2 = pSysfsAccess->read(minDefaultFreqFile, minDefault);
            if (result1 == ZE_RESULT_SUCCESS && result2 == ZE_RESULT_SUCCESS) {
                result = setMax(maxDefault);
                if (ZE_RESULT_SUCCESS != result) {
                    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                          "error@<%s> <setMax(maxDefault) returned 0x%x>\n", __func__, result);
                    return result;
                }
                return setMin(minDefault);
            }
        }
    }

    double currentMax = 0.0;
    ze_result_t result = getMax(currentMax);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getMax returned 0x%x>\n", __func__, result);
        return result;
    }
    if (newMin > currentMax) {
        // set the max first
        ze_result_t result = setMax(newMax);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "error@<%s> <setMax(newMax) returned 0x%x>\n", __func__, result);
            return result;
        }
        return setMin(newMin);
    }

    // set the min first
    result = setMin(newMin);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <setMin returned 0x%x>\n", __func__, result);
        return result;
    }
    return setMax(newMax);
}

ze_result_t LinuxFrequencyImp::osFrequencyGetState(zes_freq_state_t *pState) {
    ze_result_t result;

    result = getRequest(pState->request);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getRequest returned 0x%x>\n", __func__, result);
        pState->request = -1;
    }

    result = getTdp(pState->tdp);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getTdp returned 0x%x>\n", __func__, result);
        pState->tdp = -1;
    }

    result = getEfficient(pState->efficient);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getEfficient returned 0x%x>\n", __func__, result);
        pState->efficient = -1;
    }

    result = getActual(pState->actual);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <getActual returned 0x%x>\n", __func__, result);
        pState->actual = -1;
    }

    getCurrentVoltage(pState->currentVoltage);

    pState->throttleReasons = pSysmanProductHelper->getThrottleReasons(pSysmanKmdInterface, pSysfsAccess, subdeviceId, const_cast<void *>(pState->pNext));

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getOcCapabilities(zes_oc_capabilities_t *pOcCapabilities) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getOcFrequencyTarget(double *pCurrentOcFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::setOcFrequencyTarget(double currentOcFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getOcMode(zes_oc_mode_t *pCurrentOcMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::setOcMode(zes_oc_mode_t currentOcMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getOcIccMax(double *pOcIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::setOcIccMax(double ocIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getOcTjMax(double *pOcTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::setOcTjMax(double ocTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getMin(double &min) {
    double freqVal = 0;
    ze_result_t result = pSysfsAccess->read(minFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, minFreqFile.c_str(), result);
        return result;
    }
    min = freqVal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::setMin(double min) {
    ze_result_t result = pSysfsAccess->write(minFreqFile, min);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to write file %s> <result: 0x%x>\n", __func__, minFreqFile.c_str(), result);
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMax(double &max) {
    double freqVal = 0;
    ze_result_t result = pSysfsAccess->read(maxFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, maxFreqFile.c_str(), result);
        return result;
    }
    max = freqVal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::setMax(double max) {
    ze_result_t result = pSysfsAccess->write(maxFreqFile, max);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to write file %s> <result: 0x%x>\n", __func__, maxFreqFile.c_str(), result);
        return result;
    }

    if (pSysmanKmdInterface->isBoostFrequencyAvailable()) {
        result = pSysfsAccess->write(boostFreqFile, max);
    }

    return result;
}

ze_result_t LinuxFrequencyImp::getRequest(double &request) {
    double freqVal = 0;

    ze_result_t result = pSysfsAccess->read(requestFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, requestFreqFile.c_str(), result);
        return result;
    }
    request = freqVal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getTdp(double &tdp) {
    ze_result_t result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    double freqVal = 0;

    if (pSysmanKmdInterface->isTdpFrequencyAvailable()) {
        result = pSysfsAccess->read(tdpFreqFile, freqVal);
        if (ZE_RESULT_SUCCESS != result) {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, tdpFreqFile.c_str(), result);
            return result;
        }
        tdp = freqVal;
    } else {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return result;
}

ze_result_t LinuxFrequencyImp::getActual(double &actual) {
    double freqVal = 0;

    ze_result_t result = pSysfsAccess->read(actualFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, actualFreqFile.c_str(), result);
        return result;
    }
    actual = freqVal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getEfficient(double &efficient) {
    double freqVal = 0;

    ze_result_t result = pSysfsAccess->read(efficientFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, efficientFreqFile.c_str(), result);
        return result;
    }
    efficient = freqVal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMaxVal(double &maxVal) {
    double freqVal = 0;

    ze_result_t result = pSysfsAccess->read(maxValFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, maxValFreqFile.c_str(), result);
        return result;
    }
    maxVal = freqVal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMinVal(double &minVal) {
    double freqVal = 0;

    ze_result_t result = pSysfsAccess->read(minValFreqFile, freqVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, minValFreqFile.c_str(), result);
        return result;
    }
    minVal = freqVal;
    return ZE_RESULT_SUCCESS;
}

void LinuxFrequencyImp::getCurrentVoltage(double &voltage) {
    voltage = -1.0;
}

void LinuxFrequencyImp::init() {

    const std::string baseDir = pSysmanKmdInterface->getBasePathForFreqDomain(subdeviceId, frequencyDomainNumber);
    bool baseDirectoryExists = false;

    if (pSysfsAccess->directoryExists(baseDir)) {
        baseDirectoryExists = true;
    }

    minFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMinFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    maxFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMaxFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    requestFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameCurrentFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    actualFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameActualFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    efficientFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameEfficientFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    maxValFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMaxValueFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    minValFreqFile = pSysmanKmdInterface->getSysfsPathForFreqDomain(SysfsName::sysfsNameMinValueFrequency, subdeviceId, baseDirectoryExists, frequencyDomainNumber);
    canControl = pSysmanProductHelper->isFrequencySetRangeSupported();

    if (pSysmanKmdInterface->isDefaultFrequencyAvailable()) {
        minDefaultFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinDefaultFrequency, subdeviceId, baseDirectoryExists);
        maxDefaultFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxDefaultFrequency, subdeviceId, baseDirectoryExists);
    }

    if (pSysmanKmdInterface->isBoostFrequencyAvailable()) {
        boostFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameBoostFrequency, subdeviceId, baseDirectoryExists);
    }

    if (pSysmanKmdInterface->isTdpFrequencyAvailable()) {
        tdpFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameTdpFrequency, subdeviceId, baseDirectoryExists);
    }
}

LinuxFrequencyImp::LinuxFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) : isSubdevice(onSubdevice), subdeviceId(subdeviceId), frequencyDomainNumber(frequencyDomainNumber) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    init();
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    LinuxFrequencyImp *pLinuxFrequencyImp = new LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, frequencyDomainNumber);
    return static_cast<OsFrequency *>(pLinuxFrequencyImp);
}

std::vector<zes_freq_domain_t> OsFrequency::getNumberOfFreqDomainsSupported(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto areImagesSupported = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().getHardwareInfo()->capabilityTable.supportsImages;
    std::vector<zes_freq_domain_t> freqDomains = {};
    if (areImagesSupported) {
        auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
        auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
        auto baseDir = pSysmanKmdInterface->getFreqMediaDomainBasePath();
        if (pSysfsAccess->directoryExists(std::move(baseDir))) {
            freqDomains.push_back(ZES_FREQ_DOMAIN_MEDIA);
        }
    }
    freqDomains.push_back(ZES_FREQ_DOMAIN_GPU);
    return freqDomains;
}

} // namespace Sysman
} // namespace L0

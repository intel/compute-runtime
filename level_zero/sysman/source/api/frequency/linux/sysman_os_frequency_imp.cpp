/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/frequency/linux/sysman_os_frequency_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <cmath>

namespace L0 {
namespace Sysman {

const bool LinuxFrequencyImp::canControl = true; // canControl is true on i915 (GEN9 Hardcode)

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
    return 50.0 / 3; // Step of 16.6666667 Mhz (GEN9 Hardcode);
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

bool LinuxFrequencyImp::getThrottleReasonStatus(void) {
    uint32_t val = 0;
    auto result = pSysfsAccess->read(throttleReasonStatusFile, val);
    if (ZE_RESULT_SUCCESS == result) {
        return (val == 0 ? false : true);
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, throttleReasonStatusFile.c_str(), result);
        return false;
    }
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

    pState->pNext = nullptr;
    pState->currentVoltage = -1.0;
    pState->throttleReasons = 0u;
    if (getThrottleReasonStatus()) {
        uint32_t val = 0;
        ze_result_t result;
        result = pSysfsAccess->read(throttleReasonPL1File, val);
        if (val && (result == ZE_RESULT_SUCCESS)) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP;
        }
        result = pSysfsAccess->read(throttleReasonPL2File, val);
        if (val && (result == ZE_RESULT_SUCCESS)) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP;
        }
        result = pSysfsAccess->read(throttleReasonPL4File, val);
        if (val && (result == ZE_RESULT_SUCCESS)) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT;
        }
        result = pSysfsAccess->read(throttleReasonThermalFile, val);
        if (val && (result == ZE_RESULT_SUCCESS)) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT;
        }
    }
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
    double intval;

    ze_result_t result = pSysfsAccess->read(minFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, minFreqFile.c_str(), result);
        return result;
    }
    min = intval;
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
    double intval;

    ze_result_t result = pSysfsAccess->read(maxFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, maxFreqFile.c_str(), result);
        return result;
    }
    max = intval;
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
    double intval;

    ze_result_t result = pSysfsAccess->read(requestFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, requestFreqFile.c_str(), result);
        return result;
    }
    request = intval;
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
    double intval;

    ze_result_t result = pSysfsAccess->read(actualFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, actualFreqFile.c_str(), result);
        return result;
    }
    actual = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getEfficient(double &efficient) {
    double intval;

    ze_result_t result = pSysfsAccess->read(efficientFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, efficientFreqFile.c_str(), result);
        return result;
    }
    efficient = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMaxVal(double &maxVal) {
    double intval;

    ze_result_t result = pSysfsAccess->read(maxValFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, maxValFreqFile.c_str(), result);
        return result;
    }
    maxVal = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMinVal(double &minVal) {
    double intval;

    ze_result_t result = pSysfsAccess->read(minValFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, minValFreqFile.c_str(), result);
        return result;
    }
    minVal = intval;
    return ZE_RESULT_SUCCESS;
}

void LinuxFrequencyImp::init() {
    const std::string baseDir = pSysmanKmdInterface->getBasePath(subdeviceId);
    bool baseDirectoryExists = pSysfsAccess->directoryExists(baseDir);

    minFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinFrequency, subdeviceId, baseDirectoryExists);
    maxFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxFrequency, subdeviceId, baseDirectoryExists);
    requestFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCurrentFrequency, subdeviceId, baseDirectoryExists);
    actualFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameActualFrequency, subdeviceId, baseDirectoryExists);
    efficientFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEfficientFrequency, subdeviceId, baseDirectoryExists);
    maxValFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxValueFrequency, subdeviceId, baseDirectoryExists);
    minValFreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinValueFrequency, subdeviceId, baseDirectoryExists);
    throttleReasonStatusFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, subdeviceId, baseDirectoryExists);
    throttleReasonPL1File = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL1, subdeviceId, baseDirectoryExists);
    throttleReasonPL2File = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL2, subdeviceId, baseDirectoryExists);
    throttleReasonPL4File = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL4, subdeviceId, baseDirectoryExists);
    throttleReasonThermalFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonThermal, subdeviceId, baseDirectoryExists);

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
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    init();
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    LinuxFrequencyImp *pLinuxFrequencyImp = new LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, frequencyDomainNumber);
    return static_cast<OsFrequency *>(pLinuxFrequencyImp);
}

std::vector<zes_freq_domain_t> OsFrequency::getNumberOfFreqDomainsSupported(OsSysman *pOsSysman) {
    std::vector<zes_freq_domain_t> freqDomains = {ZES_FREQ_DOMAIN_GPU};
    return freqDomains;
}

} // namespace Sysman
} // namespace L0

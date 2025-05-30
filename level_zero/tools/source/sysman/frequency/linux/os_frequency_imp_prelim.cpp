/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/linux/os_frequency_imp_prelim.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "neo_igfxfmid.h"

#include <cmath>

namespace L0 {

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
    auto productFamily = pDevice->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    double stepSize;
    if (productFamily >= IGFX_XE_HP_SDV) {
        stepSize = 50.0;
    } else {
        stepSize = 50.0 / 3; // Step of 16.6666667 Mhz (GEN9 Hardcode)
    }
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
    double intval = 0;
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
    double intval = 0;
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
    return pSysfsAccess->write(boostFreqFile, max);
}

ze_result_t LinuxFrequencyImp::getRequest(double &request) {
    double intval = 0;

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
    double intval = 0;

    ze_result_t result = pSysfsAccess->read(tdpFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, tdpFreqFile.c_str(), result);
        return result;
    }
    tdp = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getActual(double &actual) {
    double intval = 0;

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
    double intval = 0;

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
    double intval = 0;

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
    double intval = 0;

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
    const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
    if (pSysfsAccess->directoryExists(baseDir)) {
        minFreqFile = baseDir + "rps_min_freq_mhz";
        minDefaultFreqFile = baseDir + ".defaults/rps_min_freq_mhz";
        maxFreqFile = baseDir + "rps_max_freq_mhz";
        boostFreqFile = baseDir + "rps_boost_freq_mhz";
        maxDefaultFreqFile = baseDir + ".defaults/rps_max_freq_mhz";
        requestFreqFile = baseDir + "punit_req_freq_mhz";
        tdpFreqFile = baseDir + "rapl_PL1_freq_mhz";
        actualFreqFile = baseDir + "rps_act_freq_mhz";
        efficientFreqFile = baseDir + "rps_RP1_freq_mhz";
        maxValFreqFile = baseDir + "rps_RP0_freq_mhz";
        minValFreqFile = baseDir + "rps_RPn_freq_mhz";
        throttleReasonStatusFile = baseDir + "throttle_reason_status";
        throttleReasonPL1File = baseDir + "throttle_reason_pl1";
        throttleReasonPL2File = baseDir + "throttle_reason_pl2";
        throttleReasonPL4File = baseDir + "throttle_reason_pl4";
        throttleReasonThermalFile = baseDir + "throttle_reason_thermal";
    } else {
        minFreqFile = "gt_min_freq_mhz";
        maxFreqFile = "gt_max_freq_mhz";
        boostFreqFile = "gt_boost_freq_mhz";
        requestFreqFile = "gt_cur_freq_mhz";
        tdpFreqFile = "rapl_PL1_freq_mhz";
        actualFreqFile = "gt_act_freq_mhz";
        efficientFreqFile = "gt_RP1_freq_mhz";
        maxValFreqFile = "gt_RP0_freq_mhz";
        minValFreqFile = "gt_RPn_freq_mhz";
        throttleReasonStatusFile = "gt_throttle_reason_status";
        throttleReasonPL1File = "gt_throttle_reason_status_pl1";
        throttleReasonPL2File = "gt_throttle_reason_status_pl2";
        throttleReasonPL4File = "gt_throttle_reason_status_pl4";
        throttleReasonThermalFile = "gt_throttle_reason_status_thermal";
    }
}

LinuxFrequencyImp::LinuxFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) : isSubdevice(onSubdevice), subdeviceId(subdeviceId), frequencyDomainNumber(frequencyDomainNumber) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pDevice = Device::fromHandle(pLinuxSysmanImp->getSysmanDeviceImp()->hCoreDevice);
    init();
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    LinuxFrequencyImp *pLinuxFrequencyImp = new LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, frequencyDomainNumber);
    return static_cast<OsFrequency *>(pLinuxFrequencyImp);
}

std::vector<zes_freq_domain_t> OsFrequency::getNumberOfFreqDomainsSupported(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pDevice = Device::fromHandle(pLinuxSysmanImp->getSysmanDeviceImp()->hCoreDevice);
    auto areImagesSupported = pDevice->getHwInfo().capabilityTable.supportsImages;
    std::vector<zes_freq_domain_t> freqDomains = {};
    if (areImagesSupported) {
        auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
        const std::string baseDir = "gt/gt1/";
        if (pSysfsAccess->directoryExists(baseDir)) {
            freqDomains.push_back(ZES_FREQ_DOMAIN_MEDIA);
        }
    }
    freqDomains.push_back(ZES_FREQ_DOMAIN_GPU);
    return freqDomains;
}

} // namespace L0

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/performance/linux/os_performance_imp_prelim.h"

#include <cmath>

namespace L0 {
constexpr double maxPerformanceFactor = 100;
constexpr double halfOfMaxPerformanceFactor = 50;
constexpr double minPerformanceFactor = 0;
const std::string LinuxPerformanceImp::sysPwrBalance("sys_pwr_balance");

ze_result_t LinuxPerformanceImp::osPerformanceGetProperties(zes_perf_properties_t &pProperties) {
    pProperties.onSubdevice = isSubdevice;
    pProperties.subdeviceId = subdeviceId;
    pProperties.engines = domain;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPerformanceImp::getBaseScaleFactor() {
    auto result = pSysfsAccess->read(baseScale, baseScaleReading);
    if (ZE_RESULT_SUCCESS != result) {
        return getErrorCode(result);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPerformanceImp::getMediaScaleFactor() {
    auto result = pSysfsAccess->read(mediaScale, mediaScaleReading);
    if (ZE_RESULT_SUCCESS != result) {
        return getErrorCode(result);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPerformanceImp::osPerformanceGetConfig(double *pFactor) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double baseFactorReading = 0;
    double mediaFactorReading = 0;
    double sysPwrBalanceReading = 0;
    double multiplier = 0;
    switch (domain) {
    case ZES_ENGINE_TYPE_FLAG_OTHER:
        result = pSysfsAccess->read(sysPwrBalance, sysPwrBalanceReading);
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }
        if (sysPwrBalanceReading >= 0 && sysPwrBalanceReading <= 16.0) {
            *pFactor = 50.0 + std::round((16.0 - sysPwrBalanceReading) * 50.0 / 16.0);
        } else if (sysPwrBalanceReading > 16.0 && sysPwrBalanceReading <= 63.0) {
            *pFactor = std::round((63.0 - sysPwrBalanceReading) * 50.0 / (63.0 - 16.0));
        } else {
            result = ZE_RESULT_ERROR_UNKNOWN;
        }
        break;
    case ZES_ENGINE_TYPE_FLAG_MEDIA:
        result = pSysfsAccess->read(mediaFreqFactor, mediaFactorReading);
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }
        multiplier = (mediaFactorReading * mediaScaleReading); // Value retrieved from media_freq_factor file is in U(fixed point decimal) format convert it into decimal by multiplication with scale factor
        if (multiplier == 1) {
            *pFactor = maxPerformanceFactor;
        } else if (multiplier == 0.5) {
            *pFactor = minPerformanceFactor;
        } else {
            result = ZE_RESULT_ERROR_UNKNOWN;
        }
        break;
    case ZES_ENGINE_TYPE_FLAG_COMPUTE:
        result = pSysfsAccess->read(baseFreqFactor, baseFactorReading);
        if (ZE_RESULT_SUCCESS != result) {
            return getErrorCode(result);
        }
        multiplier = (baseFactorReading * baseScaleReading); // Value retrieved from base_freq_factor file is in U(fixed point decimal) format convert it into decimal by multiplication with scale factor
        if (multiplier >= 0.5 && multiplier <= 1) {
            *pFactor = (1 - multiplier) * 100 + 50;
        } else if (multiplier > 1 && multiplier <= 2) {
            *pFactor = (2 - multiplier) * 50;
        } else {
            result = ZE_RESULT_ERROR_UNKNOWN;
        }
        break;
    default:
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }
    return result;
}

ze_result_t LinuxPerformanceImp::osPerformanceSetConfig(double pFactor) {
    double multiplier = 0;
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    if (pFactor < minPerformanceFactor || pFactor > maxPerformanceFactor) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    switch (domain) {
    case ZES_ENGINE_TYPE_FLAG_OTHER:
        if (pFactor <= halfOfMaxPerformanceFactor) {
            multiplier = 63.0 - std::round(pFactor * (47.0) / 50.0); // multiplier = 63 - ROUND(pFactor * (63.0 - 16.0) / 50.0)
        } else {
            multiplier = 16.0 - std::round((pFactor - 50.0) * 16.0 / 50.0);
        }
        result = pSysfsAccess->write(sysPwrBalance, multiplier);
        break;
    case ZES_ENGINE_TYPE_FLAG_MEDIA:
        if (pFactor < halfOfMaxPerformanceFactor) {
            multiplier = 0.5;
        } else {
            multiplier = 1;
        }
        multiplier = multiplier / mediaScaleReading; // Divide by scale factor and then round off to convert from decimal to U format
        multiplier = std::round(multiplier);
        result = pSysfsAccess->write(mediaFreqFactor, multiplier);
        break;
    case ZES_ENGINE_TYPE_FLAG_COMPUTE:
        if (pFactor < halfOfMaxPerformanceFactor) {
            multiplier = 2 - (pFactor / 50.0);
        } else {
            multiplier = 1 - ((pFactor - 50) / 100.0);
        }
        multiplier = multiplier / baseScaleReading; // Divide by scale factor and then round off to convert from decimal to U format
        multiplier = std::round(multiplier);
        result = pSysfsAccess->write(baseFreqFactor, multiplier);
        break;
    default:
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    return result;
}

bool LinuxPerformanceImp::isPerformanceSupported(void) {
    switch (domain) {
    case ZES_ENGINE_TYPE_FLAG_OTHER:
        if (pSysfsAccess->canRead(sysPwrBalance) != ZE_RESULT_SUCCESS) {
            return false;
        }
        break;
    case ZES_ENGINE_TYPE_FLAG_MEDIA:
        if (pSysfsAccess->canRead(mediaFreqFactor) != ZE_RESULT_SUCCESS) {
            return false;
        }
        if (getMediaScaleFactor() != ZE_RESULT_SUCCESS) {
            return false;
        }
        break;
    case ZES_ENGINE_TYPE_FLAG_COMPUTE:
        if (pSysfsAccess->canRead(baseFreqFactor) != ZE_RESULT_SUCCESS) {
            return false;
        }
        if (getBaseScaleFactor() != ZE_RESULT_SUCCESS) {
            return false;
        }
        break;
    default:
        return false;
        break;
    }
    return true;
}

void LinuxPerformanceImp::init() {
    const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
    baseFreqFactor = baseDir + "base_freq_factor";
    mediaFreqFactor = baseDir + "media_freq_factor";
    baseScale = baseDir + "base_freq_factor.scale";
    mediaScale = baseDir + "media_freq_factor.scale";
}

LinuxPerformanceImp::LinuxPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                         zes_engine_type_flag_t domain) : domain(domain), subdeviceId(subdeviceId), isSubdevice(onSubdevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    init();
}

OsPerformance *OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) {
    LinuxPerformanceImp *pLinuxPerformanceImp = new LinuxPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain);
    return static_cast<OsPerformance *>(pLinuxPerformanceImp);
}

} // namespace L0

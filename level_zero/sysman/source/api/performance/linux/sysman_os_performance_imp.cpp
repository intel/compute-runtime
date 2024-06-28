/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/linux/sysman_os_performance_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include <cmath>
namespace L0 {
namespace Sysman {

static constexpr double maxSysPowerBalanceReading = 63.0;
static constexpr double minSysPowerBalanceReading = 0.0;
static constexpr double defaultSysPowerBalanceReading = 16.0;

ze_result_t LinuxPerformanceImp::getErrorCode(ze_result_t result) {
    if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return result;
}

ze_result_t LinuxPerformanceImp::osPerformanceGetProperties(zes_perf_properties_t &pProperties) {
    pProperties.onSubdevice = isSubdevice;
    pProperties.subdeviceId = subdeviceId;
    pProperties.engines = domain;
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
        result = pSysFsAccess->read(systemPowerBalance, sysPwrBalanceReading);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, systemPowerBalance.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        if (sysPwrBalanceReading >= minSysPowerBalanceReading && sysPwrBalanceReading <= defaultSysPowerBalanceReading) {
            *pFactor = halfOfMaxPerformanceFactor + std::round((defaultSysPowerBalanceReading - sysPwrBalanceReading) * halfOfMaxPerformanceFactor / defaultSysPowerBalanceReading);
        } else if (sysPwrBalanceReading > defaultSysPowerBalanceReading && sysPwrBalanceReading <= maxSysPowerBalanceReading) {
            *pFactor = std::round((maxSysPowerBalanceReading - sysPwrBalanceReading) * halfOfMaxPerformanceFactor / (maxSysPowerBalanceReading - defaultSysPowerBalanceReading));
        } else {
            result = ZE_RESULT_ERROR_UNKNOWN;
        }
        break;

    case ZES_ENGINE_TYPE_FLAG_MEDIA:
        result = pSysFsAccess->read(mediaFreqFactor, mediaFactorReading);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, mediaFreqFactor.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        multiplier = (mediaFactorReading * mediaScaleReading); // Value retrieved from media_freq_factor file is in U(fixed point decimal) format convert it into decimal by multiplication with scale factor
        if (multiplier == 1) {
            *pFactor = maxPerformanceFactor;
        } else if (multiplier == 0.5) {
            *pFactor = halfOfMaxPerformanceFactor;
        } else if (multiplier == 0) {
            *pFactor = minPerformanceFactor;
        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): multiplier for MEDIA is not matching with given presets and returning UNKNOWN ERROR \n", __FUNCTION__);
            result = ZE_RESULT_ERROR_UNKNOWN;
        }
        break;

    case ZES_ENGINE_TYPE_FLAG_COMPUTE:
        result = pSysFsAccess->read(baseFreqFactor, baseFactorReading);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, baseFreqFactor.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        multiplier = (baseFactorReading * baseScaleReading); // Value retrieved from base_freq_factor file is in U(fixed point decimal) format convert it into decimal by multiplication with scale factor
        if (multiplier >= 0.5 && multiplier <= 1) {
            *pFactor = (1 - multiplier) * maxPerformanceFactor + halfOfMaxPerformanceFactor;
        } else if (multiplier > 1 && multiplier <= 2) {
            *pFactor = (2 - multiplier) * halfOfMaxPerformanceFactor;
        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): multiplier for COMPUTE is not matching with given presets and returning UNKNOWN ERROR\n", __FUNCTION__);
            result = ZE_RESULT_ERROR_UNKNOWN;
        }
        break;

    default:
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE as osPerformanceGetConfig is not supported\n", __FUNCTION__);
        break;
    }

    return result;
}

ze_result_t LinuxPerformanceImp::osPerformanceSetConfig(double perfFactor) {
    double multiplier = 0;
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    if (perfFactor < minPerformanceFactor || perfFactor > maxPerformanceFactor) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    switch (domain) {
    case ZES_ENGINE_TYPE_FLAG_OTHER:
        if (perfFactor <= halfOfMaxPerformanceFactor) {
            multiplier = maxSysPowerBalanceReading - std::round(perfFactor * (maxSysPowerBalanceReading - defaultSysPowerBalanceReading) / halfOfMaxPerformanceFactor);
        } else {
            multiplier = defaultSysPowerBalanceReading - std::round((perfFactor - halfOfMaxPerformanceFactor) * defaultSysPowerBalanceReading / halfOfMaxPerformanceFactor);
        }
        result = pSysFsAccess->write(systemPowerBalance, multiplier);
        break;

    case ZES_ENGINE_TYPE_FLAG_MEDIA:
        pSysmanProductHelper->getMediaPerformanceFactorMultiplier(perfFactor, &multiplier);

        multiplier = multiplier / mediaScaleReading;
        multiplier = std::round(multiplier);
        result = pSysFsAccess->write(mediaFreqFactor, multiplier);
        break;

    case ZES_ENGINE_TYPE_FLAG_COMPUTE:
        if (perfFactor < halfOfMaxPerformanceFactor) {
            multiplier = 2 - (perfFactor / halfOfMaxPerformanceFactor);
        } else {
            multiplier = 1 - ((perfFactor - halfOfMaxPerformanceFactor) / maxPerformanceFactor);
        }
        multiplier = multiplier / baseScaleReading; // Divide by scale factor and then round off to convert from decimal to U format
        multiplier = std::round(multiplier);
        result = pSysFsAccess->write(baseFreqFactor, multiplier);
        break;

    default:
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE as osPerformanceSetConfig is not supported\n", __FUNCTION__);
        break;
    }

    return result;
}

ze_result_t LinuxPerformanceImp::getBaseFrequencyScaleFactor() {
    return pSysFsAccess->read(baseFreqFactorScale, baseScaleReading);
}

ze_result_t LinuxPerformanceImp::getMediaFrequencyScaleFactor() {
    return pSysFsAccess->read(mediaFreqFactorScale, mediaScaleReading);
}

bool LinuxPerformanceImp::isPerformanceSupported(void) {
    if (!pSysmanProductHelper->isPerfFactorSupported()) {
        return false;
    }

    switch (domain) {
    case ZES_ENGINE_TYPE_FLAG_OTHER:
        if (pSysmanKmdInterface->isSystemPowerBalanceAvailable() == false) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): ZES_ENGINE_TYPE_FLAG_OTHER returns false as System Power Balance is not Available\n", __FUNCTION__);
            return false;
        }
        if (pSysFsAccess->canRead(systemPowerBalance) != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): ZES_ENGINE_TYPE_FLAG_OTHER returns false SysfsAccess->canRead() failed for %s\n", __FUNCTION__, systemPowerBalance.c_str());
            return false;
        }
        break;
    case ZES_ENGINE_TYPE_FLAG_MEDIA:
        if (pSysmanKmdInterface->isMediaFrequencyFactorAvailable() == false) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): ZES_ENGINE_TYPE_FLAG_MEDIA returns false as Media Frequency is not Available\n", __FUNCTION__);
            return false;
        }
        if (pSysFsAccess->canRead(mediaFreqFactor) != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): ZES_ENGINE_TYPE_FLAG_MEDIA returns false as SysfsAccess->canRead() failed for %s\n", __FUNCTION__, mediaFreqFactor.c_str());
            return false;
        }
        if (getMediaFrequencyScaleFactor() != ZE_RESULT_SUCCESS) {
            return false;
        }
        break;
    case ZES_ENGINE_TYPE_FLAG_COMPUTE:
        if (pSysmanKmdInterface->isBaseFrequencyFactorAvailable() == false) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): ZES_ENGINE_TYPE_FLAG_COMPUTE returns false as Base Frequency is not Available\n", __FUNCTION__);
            return false;
        }
        if (pSysFsAccess->canRead(baseFreqFactor) != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): ZES_ENGINE_TYPE_FLAG_COMPUTE returns false as SysfsAccess->canRead() failed for %s\n", __FUNCTION__, baseFreqFactor.c_str());
            return false;
        }
        if (getBaseFrequencyScaleFactor() != ZE_RESULT_SUCCESS) {
            return false;
        }
        break;
    default:
        return false;
        break;
    }
    return true;
}

LinuxPerformanceImp::LinuxPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                         zes_engine_type_flag_t domain) : domain(domain), subdeviceId(subdeviceId), isSubdevice(onSubdevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysFsAccess = &pLinuxSysmanImp->getSysfsAccess();

    if (pSysmanKmdInterface->isBaseFrequencyFactorAvailable() == true) {
        baseFreqFactor = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceBaseFrequencyFactor, subdeviceId, true);
        baseFreqFactorScale = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceBaseFrequencyFactorScale, subdeviceId, true);
    }

    if (pSysmanKmdInterface->isSystemPowerBalanceAvailable() == true) {
        systemPowerBalance = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceSystemPowerBalance, subdeviceId, false);
    }

    if (pSysmanKmdInterface->isMediaFrequencyFactorAvailable() == true) {
        mediaFreqFactor = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceMediaFrequencyFactor, subdeviceId, true);
        mediaFreqFactorScale = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale, subdeviceId, true);
    }
}

OsPerformance *OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) {
    LinuxPerformanceImp *pLinuxPerformanceImp = new LinuxPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain);
    return static_cast<OsPerformance *>(pLinuxPerformanceImp);
}

} // namespace Sysman
} // namespace L0

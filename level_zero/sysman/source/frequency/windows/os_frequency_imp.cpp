/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/frequency/windows/os_frequency_imp.h"

#include "level_zero/sysman/source/frequency/frequency.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmFrequencyImp::osFrequencyGetProperties(zes_freq_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

double WddmFrequencyImp::osFrequencyGetStepSize() {
    return 0.0;
}

ze_result_t WddmFrequencyImp::osFrequencyGetRange(zes_freq_range_t *pLimits) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencySetRange(const zes_freq_range_t *pLimits) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencyGetState(zes_freq_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcCapabilities(zes_oc_capabilities_t *pOcCapabilities) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcFrequencyTarget(double *pCurrentOcFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setOcFrequencyTarget(double currentOcFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcMode(zes_oc_mode_t *pCurrentOcMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setOcMode(zes_oc_mode_t currentOcMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcIccMax(double *pOcIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setOcIccMax(double ocIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcTjMax(double *pOcTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setOcTjMax(double ocTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmFrequencyImp::WddmFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    return nullptr;
}

uint16_t OsFrequency::getNumberOfFreqDomainsSupported(OsSysman *pOsSysman) {
    return static_cast<uint16_t>(0);
}

} // namespace Sysman
} // namespace L0

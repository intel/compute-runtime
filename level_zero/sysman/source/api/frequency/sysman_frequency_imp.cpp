/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/frequency/sysman_frequency_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"

#include <algorithm>
#include <cmath>

namespace L0 {
namespace Sysman {

ze_result_t FrequencyImp::frequencyGetProperties(zes_freq_properties_t *pProperties) {
    *pProperties = zesFrequencyProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyImp::frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) {
    uint32_t numToCopy = std::min(*pCount, numClocks);
    if (0 == *pCount || *pCount > numClocks) {
        *pCount = numClocks;
    }
    if (nullptr != phFrequency) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phFrequency[i] = pClocks[i];
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyImp::frequencyGetRange(zes_freq_range_t *pLimits) {
    return pOsFrequency->osFrequencyGetRange(pLimits);
}

ze_result_t FrequencyImp::frequencySetRange(const zes_freq_range_t *pLimits) {
    double newMin = round(pLimits->min);
    double newMax = round(pLimits->max);
    // No need to check if the frequency is inside the clocks array:
    // 1. GuC will cap this, GuC has an internal range. Hw too rounds to the next step, no need to do that check.
    // 2. For Overclocking, Oc frequency will be higher than the zesFrequencyProperties.max frequency, so it would be outside
    //    the clocks array too. Pcode at the end will decide the granted frequency, no need for the check.

    if (newMin > newMax) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return pOsFrequency->osFrequencySetRange(pLimits);
}

ze_result_t FrequencyImp::frequencyGetState(zes_freq_state_t *pState) {
    return pOsFrequency->osFrequencyGetState(pState);
}

ze_result_t FrequencyImp::frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return pOsFrequency->osFrequencyGetThrottleTime(pThrottleTime);
}

ze_result_t FrequencyImp::frequencyOcGetCapabilities(zes_oc_capabilities_t *pOcCapabilities) {
    return pOsFrequency->getOcCapabilities(pOcCapabilities);
}

ze_result_t FrequencyImp::frequencyOcGetFrequencyTarget(double *pCurrentOcFrequency) {
    return pOsFrequency->getOcFrequencyTarget(pCurrentOcFrequency);
}

ze_result_t FrequencyImp::frequencyOcSetFrequencyTarget(double currentOcFrequency) {
    return pOsFrequency->setOcFrequencyTarget(currentOcFrequency);
}

ze_result_t FrequencyImp::frequencyOcGetVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) {
    return pOsFrequency->getOcVoltageTarget(pCurrentVoltageTarget, pCurrentVoltageOffset);
}

ze_result_t FrequencyImp::frequencyOcSetVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) {
    return pOsFrequency->setOcVoltageTarget(currentVoltageTarget, currentVoltageOffset);
}

ze_result_t FrequencyImp::frequencyOcGetMode(zes_oc_mode_t *pCurrentOcMode) {
    return pOsFrequency->getOcMode(pCurrentOcMode);
}

ze_result_t FrequencyImp::frequencyOcSetMode(zes_oc_mode_t currentOcMode) {
    return pOsFrequency->setOcMode(currentOcMode);
}

ze_result_t FrequencyImp::frequencyOcGetIccMax(double *pOcIccMax) {
    return pOsFrequency->getOcIccMax(pOcIccMax);
}

ze_result_t FrequencyImp::frequencyOcSetIccMax(double ocIccMax) {
    return pOsFrequency->setOcIccMax(ocIccMax);
}

ze_result_t FrequencyImp::frequencyOcGeTjMax(double *pOcTjMax) {
    return pOsFrequency->getOcTjMax(pOcTjMax);
}

ze_result_t FrequencyImp::frequencyOcSetTjMax(double ocTjMax) {
    return pOsFrequency->setOcTjMax(ocTjMax);
}

void FrequencyImp::init() {
    pOsFrequency->osFrequencyGetProperties(zesFrequencyProperties);
    double step = pOsFrequency->osFrequencyGetStepSize();
    double freqRange = zesFrequencyProperties.max - zesFrequencyProperties.min;
    numClocks = static_cast<uint32_t>(round(freqRange / step)) + 1;
    pClocks = new double[numClocks];
    for (unsigned int i = 0; i < numClocks; i++) {
        pClocks[i] = round(zesFrequencyProperties.min + (step * i));
    }
}

FrequencyImp::FrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    pOsFrequency = OsFrequency::create(pOsSysman, onSubdevice, subdeviceId, frequencyDomainNumber);
    UNRECOVERABLE_IF(nullptr == pOsFrequency);
    init();
}

FrequencyImp::~FrequencyImp() {
    delete pOsFrequency;
    delete[] pClocks;
}

} // namespace Sysman
} // namespace L0

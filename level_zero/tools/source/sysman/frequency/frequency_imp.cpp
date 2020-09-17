/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/frequency_imp.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include <cmath>

namespace L0 {

const double FrequencyImp::step = 50.0 / 3; // Step of 16.6666667 Mhz (GEN9 Hardcode)

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
    bool newMinValid = false, newMaxValid = false;
    for (unsigned int i = 0; i < numClocks; i++) {
        if (newMin == pClocks[i]) {
            newMinValid = true;
        }
        if (newMax == pClocks[i]) {
            newMaxValid = true;
        }
    }
    if (newMin > newMax || !newMinValid || !newMaxValid) {
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
    double freqRange = zesFrequencyProperties.max - zesFrequencyProperties.min;
    numClocks = static_cast<uint32_t>(round(freqRange / step)) + 1;
    pClocks = new double[numClocks];
    for (unsigned int i = 0; i < numClocks; i++) {
        pClocks[i] = round(zesFrequencyProperties.min + (step * i));
    }
}

FrequencyImp::FrequencyImp(OsSysman *pOsSysman, ze_device_handle_t handle, zes_freq_domain_t frequencyDomainNumber) : deviceHandle(handle) {
    ze_device_properties_t deviceProperties = {};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    pOsFrequency = OsFrequency::create(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId, frequencyDomainNumber);
    UNRECOVERABLE_IF(nullptr == pOsFrequency);

    init();
}

FrequencyImp::~FrequencyImp() {
    delete pOsFrequency;
    delete[] pClocks;
}

} // namespace L0

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
const bool FrequencyImp::canControl = true; // canControl is true on i915 (GEN9 Hardcode)

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
    ze_result_t result = pOsFrequency->getMax(pLimits->max);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return pOsFrequency->getMin(pLimits->min);
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
    double currentMax;
    pOsFrequency->getMax(currentMax);
    if (newMin > currentMax) {
        // set the max first
        ze_result_t result = pOsFrequency->setMax(newMax);
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }

        return pOsFrequency->setMin(newMin);
    }

    // set the min first
    ze_result_t result = pOsFrequency->setMin(newMin);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return pOsFrequency->setMax(newMax);
}

ze_result_t FrequencyImp::frequencyGetState(zes_freq_state_t *pState) {
    ze_result_t result;

    result = pOsFrequency->getRequest(pState->request);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = pOsFrequency->getTdp(pState->tdp);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = pOsFrequency->getEfficient(pState->efficient);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = pOsFrequency->getActual(pState->actual);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    pState->stype = ZES_STRUCTURE_TYPE_FREQ_STATE;
    pState->pNext = nullptr;
    pState->currentVoltage = -1.0;
    pState->throttleReasons = 0u;
    return result;
}

ze_result_t FrequencyImp::frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void FrequencyImp::init() {
    zesFrequencyProperties.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
    zesFrequencyProperties.pNext = nullptr;
    zesFrequencyProperties.type = ZES_FREQ_DOMAIN_GPU;
    zesFrequencyProperties.onSubdevice = false;
    zesFrequencyProperties.subdeviceId = 0;
    zesFrequencyProperties.canControl = canControl;
    ze_result_t result3 = pOsFrequency->getMinVal(zesFrequencyProperties.min);
    ze_result_t result4 = pOsFrequency->getMaxVal(zesFrequencyProperties.max);
    // If can't figure out the valid range, then can't control it.
    if (ZE_RESULT_SUCCESS != result3 || ZE_RESULT_SUCCESS != result4) {
        zesFrequencyProperties.canControl = false;
        zesFrequencyProperties.min = 0.0;
        zesFrequencyProperties.max = 0.0;
    }
    zesFrequencyProperties.isThrottleEventSupported = false;

    double freqRange = zesFrequencyProperties.max - zesFrequencyProperties.min;
    numClocks = static_cast<uint32_t>(round(freqRange / step)) + 1;
    pClocks = new double[numClocks];
    for (unsigned int i = 0; i < numClocks; i++) {
        pClocks[i] = round(zesFrequencyProperties.min + (step * i));
    }
}

FrequencyImp::FrequencyImp(OsSysman *pOsSysman) {
    pOsFrequency = OsFrequency::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pOsFrequency);

    init();
}

FrequencyImp::~FrequencyImp() {
    delete pOsFrequency;
    delete[] pClocks;
}

} // namespace L0

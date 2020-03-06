/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/frequency_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cmath>

namespace L0 {

const double FrequencyImp::step = 50.0 / 3; // Step of 16.6666667 Mhz (GEN9 Hardcode)
const bool FrequencyImp::canControl = true; // canControl is true on i915 (GEN9 Hardcode)

ze_result_t FrequencyImp::frequencyGetProperties(zet_freq_properties_t *pProperties) {
    *pProperties = frequencyProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyImp::frequencyGetRange(zet_freq_range_t *pLimits) {
    ze_result_t result = pOsFrequency->getMax(pLimits->max);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return pOsFrequency->getMin(pLimits->min);
}

ze_result_t FrequencyImp::frequencySetRange(const zet_freq_range_t *pLimits) {
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
        return ZE_RESULT_ERROR_UNKNOWN;
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

ze_result_t FrequencyImp::frequencyGetState(zet_freq_state_t *pState) {
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

    result = pOsFrequency->getThrottleReasons(pState->throttleReasons);
    if (ZE_RESULT_ERROR_UNKNOWN == result) {
        // Throttle Reason is optional, set to none for now.
        pState->throttleReasons = ZET_FREQ_THROTTLE_REASONS_NONE;
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

void FrequencyImp::init() {
    frequencyProperties.type = ZET_FREQ_DOMAIN_GPU;
    frequencyProperties.onSubdevice = false;
    frequencyProperties.subdeviceId = 0;
    frequencyProperties.canControl = canControl;
    ze_result_t result1 = pOsFrequency->getMinVal(frequencyProperties.min);
    ze_result_t result2 = pOsFrequency->getMaxVal(frequencyProperties.max);
    // If can't figure out the valid range, then can't control it.
    if (ZE_RESULT_SUCCESS != result1 || ZE_RESULT_SUCCESS != result2) {
        frequencyProperties.canControl = false;
        frequencyProperties.min = 0.0;
        frequencyProperties.max = 0.0;
    }
    frequencyProperties.step = step;
    double freqRange = frequencyProperties.max - frequencyProperties.min;
    numClocks = static_cast<uint32_t>(round(freqRange / frequencyProperties.step)) + 1;
    pClocks = new double[numClocks];
    for (unsigned int i = 0; i < numClocks; i++) {
        pClocks[i] = round(frequencyProperties.min + (frequencyProperties.step * i));
    }
    frequencyProperties.isThrottleEventSupported = false;
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

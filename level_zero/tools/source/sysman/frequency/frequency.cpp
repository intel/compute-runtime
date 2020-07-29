/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/frequency.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/frequency/frequency_imp.h"

namespace L0 {

FrequencyHandleContext::~FrequencyHandleContext() {
    for (Frequency *pFrequency : handleList) {
        delete pFrequency;
    }
}

ze_result_t FrequencyHandleContext::init() {
    Frequency *pFrequency = new FrequencyImp(pOsSysman);
    handleList.push_back(pFrequency);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyHandleContext::frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phFrequency) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phFrequency[i] = handleList[i]->toZesFreqHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

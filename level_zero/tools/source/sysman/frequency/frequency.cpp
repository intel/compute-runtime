/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/frequency.h"

#include "level_zero/tools/source/sysman/frequency/frequency_imp.h"

namespace L0 {

FrequencyHandleContext::~FrequencyHandleContext() {
    for (Frequency *pFrequency : handle_list) {
        delete pFrequency;
    }
}

ze_result_t FrequencyHandleContext::init() {
    Frequency *pFrequency = new FrequencyImp(pOsSysman);
    handle_list.push_back(pFrequency);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyHandleContext::frequencyGet(uint32_t *pCount, zet_sysman_freq_handle_t *phFrequency) {
    if (nullptr == phFrequency) {
        *pCount = static_cast<uint32_t>(handle_list.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (Frequency *freq : handle_list) {
        if (i >= *pCount)
            break;
        phFrequency[i++] = freq->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/frequency.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/frequency/frequency_imp.h"
#include "level_zero/tools/source/sysman/frequency/os_frequency.h"
#include "level_zero/tools/source/sysman/os_sysman.h"

namespace L0 {

FrequencyHandleContext::~FrequencyHandleContext() {
    for (Frequency *pFrequency : handleList) {
        delete pFrequency;
    }
}

void FrequencyHandleContext::createHandle(ze_device_handle_t deviceHandle, zes_freq_domain_t frequencyDomain) {
    Frequency *pFrequency = new FrequencyImp(pOsSysman, deviceHandle, frequencyDomain);
    handleList.push_back(pFrequency);
}

ze_result_t FrequencyHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    for (const auto &deviceHandle : deviceHandles) {
        auto totalDomains = OsFrequency::getNumberOfFreqDoainsSupported(pOsSysman);
        UNRECOVERABLE_IF(totalDomains > 2);
        for (uint32_t frequencyDomain = 0; frequencyDomain < totalDomains; frequencyDomain++) {
            createHandle(deviceHandle, static_cast<zes_freq_domain_t>(frequencyDomain));
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyHandleContext::frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    std::call_once(initFrequencyOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles());
    });
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

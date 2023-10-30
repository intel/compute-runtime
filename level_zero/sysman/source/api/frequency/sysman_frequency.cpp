/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/frequency/sysman_frequency.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/frequency/sysman_frequency_imp.h"
#include "level_zero/sysman/source/api/frequency/sysman_os_frequency.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {

FrequencyHandleContext::~FrequencyHandleContext() {
    for (Frequency *pFrequency : handleList) {
        delete pFrequency;
    }
}

void FrequencyHandleContext::createHandle(ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomain) {
    Frequency *pFrequency = new FrequencyImp(pOsSysman, onSubdevice, subdeviceId, frequencyDomain);
    handleList.push_back(pFrequency);
}

ze_result_t FrequencyHandleContext::init(uint32_t subDeviceCount) {

    auto totalDomains = OsFrequency::getNumberOfFreqDomainsSupported(pOsSysman);
    UNRECOVERABLE_IF(totalDomains.size() > 3);
    if (subDeviceCount == 0) {
        for (const auto &frequencyDomain : totalDomains) {
            createHandle(false, 0, static_cast<zes_freq_domain_t>(frequencyDomain));
        }
    }

    else {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            for (const auto &frequencyDomain : totalDomains) {
                createHandle(true, subDeviceId, static_cast<zes_freq_domain_t>(frequencyDomain));
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t FrequencyHandleContext::frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    std::call_once(initFrequencyOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
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

} // namespace Sysman
} // namespace L0

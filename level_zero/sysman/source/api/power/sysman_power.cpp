/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/power/sysman_power.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/power/sysman_os_power.h"
#include "level_zero/sysman/source/api/power/sysman_power_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {

void PowerHandleContext::releasePowerHandles() {
    for (Power *pPower : handleList) {
        delete pPower;
    }
    handleList.clear();
}

PowerHandleContext::~PowerHandleContext() {
    releasePowerHandles();
}

void PowerHandleContext::createHandle(ze_bool_t isSubDevice, uint32_t subDeviceId, zes_power_domain_t powerDomain) {
    Power *pPower = new PowerImp(pOsSysman, isSubDevice, subDeviceId, powerDomain);
    if (pPower->initSuccess == true) {
        handleList.push_back(pPower);
    } else {
        delete pPower;
    }
}

void PowerHandleContext::init(uint32_t subDeviceCount) {
    auto totalDomains = OsPower::getSupportedPowerDomains(pOsSysman);
    for (auto &powerDomain : totalDomains) {
        createHandle(false, 0, powerDomain);
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            createHandle(true, subDeviceId, powerDomain);
        }
    }
}

void PowerHandleContext::initPower() {
    std::call_once(initPowerOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
        this->powerInitDone = true;
    });
}

ze_result_t PowerHandleContext::powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) {
    initPower();
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phPower) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phPower[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t PowerHandleContext::powerGetCardDomain(zes_pwr_handle_t *phPower) {
    initPower();
    if (nullptr == phPower) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    for (uint32_t i = 0; i < handleList.size(); i++) {
        if (handleList[i]->isCardPower) {
            *phPower = handleList[i]->toHandle();
            return ZE_RESULT_SUCCESS;
        }
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0

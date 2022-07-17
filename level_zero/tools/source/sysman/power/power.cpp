/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/power.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/power/power_imp.h"

namespace L0 {

PowerHandleContext::~PowerHandleContext() {
    for (Power *pPower : handleList) {
        delete pPower;
    }
}

void PowerHandleContext::createHandle(ze_device_handle_t deviceHandle) {
    Power *pPower = new PowerImp(pOsSysman, deviceHandle);
    if (pPower->initSuccess == true) {
        handleList.push_back(pPower);
    } else {
        delete pPower;
    }
}
ze_result_t PowerHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles, ze_device_handle_t coreDevice) {
    // Create Handle for card level power
    if (deviceHandles.size() > 1) {
        createHandle(coreDevice);
    }

    for (const auto &deviceHandle : deviceHandles) {
        createHandle(deviceHandle);
    }
    return ZE_RESULT_SUCCESS;
}

void PowerHandleContext::initPower() {
    std::call_once(initPowerOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles(), pOsSysman->getCoreDeviceHandle());
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

} // namespace L0

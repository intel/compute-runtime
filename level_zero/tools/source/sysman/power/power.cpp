/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/power.h"

#include "shared/source/helpers/basic_math.h"

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
    // Create Handle for device level power
    if (deviceHandles.size() > 1) {
        createHandle(coreDevice);
    }

    for (const auto &deviceHandle : deviceHandles) {
        createHandle(deviceHandle);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t PowerHandleContext::powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) {
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

} // namespace L0

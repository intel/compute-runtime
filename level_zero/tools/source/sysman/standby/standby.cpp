/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby.h"

#include "shared/source/helpers/basic_math.h"

#include "standby_imp.h"

namespace L0 {

StandbyHandleContext::~StandbyHandleContext() {
    for (Standby *pStandby : handleList) {
        delete pStandby;
    }
}

void StandbyHandleContext::createHandle(ze_device_handle_t deviceHandle) {
    Standby *pStandby = new StandbyImp(pOsSysman, deviceHandle);
    if (pStandby->isStandbyEnabled == true) {
        handleList.push_back(pStandby);
    } else {
        delete pStandby;
    }
}

ze_result_t StandbyHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    for (const auto &deviceHandle : deviceHandles) {
        createHandle(deviceHandle);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t StandbyHandleContext::standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) {
    std::call_once(initStandbyOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles());
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phStandby) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phStandby[i] = handleList[i]->toStandbyHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}
} // namespace L0

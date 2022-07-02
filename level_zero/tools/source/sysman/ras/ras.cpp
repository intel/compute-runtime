/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/ras/ras_imp.h"

namespace L0 {

void RasHandleContext::releaseRasHandles() {
    for (Ras *pRas : handleList) {
        delete pRas;
    }
    handleList.clear();
}

RasHandleContext::~RasHandleContext() {
    releaseRasHandles();
}

void RasHandleContext::createHandle(zes_ras_error_type_t type, ze_device_handle_t deviceHandle) {
    Ras *pRas = new RasImp(pOsSysman, type, deviceHandle);
    handleList.push_back(pRas);
}

void RasHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    for (const auto &deviceHandle : deviceHandles) {
        std::set<zes_ras_error_type_t> errorType = {};
        OsRas::getSupportedRasErrorTypes(errorType, pOsSysman, deviceHandle);
        for (const auto &type : errorType) {
            createHandle(type, deviceHandle);
        }
    }
}
ze_result_t RasHandleContext::rasGet(uint32_t *pCount,
                                     zes_ras_handle_t *phRas) {
    std::call_once(initRasOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles());
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phRas) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phRas[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/ras/ras.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/os_sysman.h"

namespace L0 {
namespace Sysman {

void RasHandleContext::releaseRasHandles() {
    for (Ras *pRas : handleList) {
        delete pRas;
    }
    handleList.clear();
}

RasHandleContext::~RasHandleContext() {
    releaseRasHandles();
}

void RasHandleContext::init(uint32_t subDeviceCount) {
}

ze_result_t RasHandleContext::rasGet(uint32_t *pCount,
                                     zes_ras_handle_t *phRas) {
    std::call_once(initRasOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
        this->rasInitDone = true;
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

} // namespace Sysman
} // namespace L0

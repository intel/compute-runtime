/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fan/sysman_fan.h"

#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"

namespace L0 {
namespace Sysman {

FanHandleContext::~FanHandleContext() = default;

void FanHandleContext::createHandle(uint32_t fanIndex, bool multipleFansSupported) {
    std::unique_ptr<Fan> pFan = std::make_unique<FanImp>(pOsSysman, fanIndex, multipleFansSupported);
    handleList.push_back(std::move(pFan));
}

void FanHandleContext::init() {
    auto supportedFanCount = OsFan::getSupportedFanCount(pOsSysman);
    bool multipleFansSupported = (supportedFanCount > 1);

    for (uint32_t fanIndex = 0; fanIndex < supportedFanCount; fanIndex++) {
        createHandle(fanIndex, multipleFansSupported);
    }
}

ze_result_t FanHandleContext::fanGet(uint32_t *pCount, zes_fan_handle_t *phFan) {
    std::call_once(initFanOnce, [this]() {
        this->init();
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phFan) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phFan[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/fan.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/fan/fan_imp.h"

namespace L0 {

FanHandleContext::~FanHandleContext() = default;

void FanHandleContext::init() {
    std::unique_ptr<Fan> pFan = std::make_unique<FanImp>(pOsSysman);
    if (pFan->initSuccess == true) {
        handleList.push_back(std::move(pFan));
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

} // namespace L0

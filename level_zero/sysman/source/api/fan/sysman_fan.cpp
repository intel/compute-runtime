/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fan/sysman_fan.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"

namespace L0 {
namespace Sysman {

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
    if (0 == *pCount) {
        *pCount = handleListSize;
        return ZE_RESULT_SUCCESS;
    }
    *pCount = std::min(*pCount, handleListSize);
    if (nullptr != phFan) {
        for (uint32_t i = 0; i < *pCount; i++) {
            phFan[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2019-2020 Intel Corporation
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

void StandbyHandleContext::init() {
    Standby *pStandby = new StandbyImp(pOsSysman);
    if (pStandby->isStandbyEnabled == true) {
        handleList.push_back(pStandby);
    } else {
        delete pStandby;
    }
}

ze_result_t StandbyHandleContext::standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) {
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

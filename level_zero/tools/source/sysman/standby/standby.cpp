/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby.h"

#include "standby_imp.h"

namespace L0 {

StandbyHandleContext::~StandbyHandleContext() {
    for (Standby *pStandby : handle_list) {
        delete pStandby;
    }
}

ze_result_t StandbyHandleContext::init() {
    Standby *pStandby = new StandbyImp(pOsSysman);
    handle_list.push_back(pStandby);
    return ZE_RESULT_SUCCESS;
}

ze_result_t StandbyHandleContext::standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby) {
    if (nullptr == phStandby) {
        *pCount = static_cast<uint32_t>(handle_list.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (Standby *standby : handle_list) {
        if (i >= *pCount)
            break;
        phStandby[i++] = standby->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

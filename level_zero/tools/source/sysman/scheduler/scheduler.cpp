/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"

namespace L0 {

SchedulerHandleContext::~SchedulerHandleContext() {
    for (Scheduler *pScheduler : handleList) {
        delete pScheduler;
    }
}

void SchedulerHandleContext::init() {
    Scheduler *pScheduler = new SchedulerImp(pOsSysman);
    if (pScheduler->initSuccess == true) {
        handleList.push_back(pScheduler);
    } else {
        delete pScheduler;
    }
}

ze_result_t SchedulerHandleContext::schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) {
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phScheduler) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phScheduler[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

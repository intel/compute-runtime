/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"

#include <algorithm>

class OsScheduler;
namespace L0 {

SchedulerHandleContext::SchedulerHandleContext(OsSysman *pOsSysman) {
    this->pOsSysman = pOsSysman;
}

SchedulerHandleContext::~SchedulerHandleContext() {
    handleList.clear();
}
void SchedulerHandleContext::createHandle(zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines, ze_device_handle_t deviceHandle) {
    std::unique_ptr<Scheduler> pScheduler = std::make_unique<SchedulerImp>(pOsSysman, engineType, listOfEngines, deviceHandle);
    handleList.push_back(std::move(pScheduler));
}

void SchedulerHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    for (const auto &deviceHandle : deviceHandles) {
        std::map<zes_engine_type_flag_t, std::vector<std::string>> engineTypeInstance = {};
        OsScheduler::getNumEngineTypeAndInstances(engineTypeInstance, pOsSysman, deviceHandle);
        for (auto itr = engineTypeInstance.begin(); itr != engineTypeInstance.end(); ++itr) {
            createHandle(itr->first, itr->second, deviceHandle);
        }
    }
}

ze_result_t SchedulerHandleContext::schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) {
    std::call_once(initSchedulerOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles());
    });
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

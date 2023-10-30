/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/scheduler/sysman_scheduler_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

SchedulerHandleContext::SchedulerHandleContext(OsSysman *pOsSysman) {
    this->pOsSysman = pOsSysman;
}

SchedulerHandleContext::~SchedulerHandleContext() {
    handleList.clear();
}

void SchedulerHandleContext::createHandle(zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines, ze_bool_t onSubDevice, uint32_t subDeviceId) {
    std::unique_ptr<Scheduler> pScheduler = std::make_unique<SchedulerImp>(pOsSysman, engineType, listOfEngines, onSubDevice, subDeviceId);
    handleList.push_back(std::move(pScheduler));
}

void SchedulerHandleContext::init(uint32_t subDeviceCount) {

    if (subDeviceCount > 0) {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            std::map<zes_engine_type_flag_t, std::vector<std::string>> engineTypeInstance = {};
            OsScheduler::getNumEngineTypeAndInstances(engineTypeInstance, pOsSysman, true, subDeviceId);
            for (auto itr = engineTypeInstance.begin(); itr != engineTypeInstance.end(); ++itr) {
                createHandle(itr->first, itr->second, true, subDeviceId);
            }
        }
    } else {
        std::map<zes_engine_type_flag_t, std::vector<std::string>> engineTypeInstance = {};
        OsScheduler::getNumEngineTypeAndInstances(engineTypeInstance, pOsSysman, false, 0);
        for (auto itr = engineTypeInstance.begin(); itr != engineTypeInstance.end(); ++itr) {
            createHandle(itr->first, itr->second, false, 0);
        }
    }
}

ze_result_t SchedulerHandleContext::schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) {
    std::call_once(initSchedulerOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
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

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/sysman_const.h"

namespace L0 {

ze_result_t SchedulerImp::setExclusiveMode(ze_bool_t *pNeedReload) {
    uint64_t timeslice = 0, timeout = 0, heartbeat = 0;
    *pNeedReload = false;
    ze_result_t result = pOsScheduler->setPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->setTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->setHeartbeatInterval(heartbeat);
    return result;
}

ze_result_t SchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    auto result = setExclusiveMode(pNeedReload);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->setComputeUnitDebugMode(pNeedReload);
    return result;
}

ze_result_t SchedulerImp::getCurrentMode(zes_sched_mode_t *pMode) {
    uint64_t timeout = 0;
    uint64_t timeslice = 0;
    ze_result_t result = pOsScheduler->getPreemptTimeout(timeout, false);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->getTimesliceDuration(timeslice, false);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    if (timeslice > 0) {
        *pMode = ZES_SCHED_MODE_TIMESLICE;
    } else {
        if (timeout > 0) {
            *pMode = ZES_SCHED_MODE_TIMEOUT;
        } else {
            *pMode = ZES_SCHED_MODE_EXCLUSIVE;
        }
    }
    return result;
}

ze_result_t SchedulerImp::getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) {
    uint64_t heartbeat = 0;
    ze_result_t result = pOsScheduler->getHeartbeatInterval(heartbeat, getDefaults);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pConfig->watchdogTimeout = heartbeat;

    return result;
}

ze_result_t SchedulerImp::getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) {
    uint64_t timeout = 0, timeslice = 0;
    ze_result_t result = pOsScheduler->getPreemptTimeout(timeout, getDefaults);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->getTimesliceDuration(timeslice, getDefaults);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pConfig->interval = timeslice;
    pConfig->yieldTimeout = timeout;
    return result;
}

ze_result_t SchedulerImp::setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) {
    zes_sched_mode_t currMode;
    ze_result_t result = getCurrentMode(&currMode);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    if (pProperties->watchdogTimeout < minTimeoutModeHeartbeat) {
        // watchdogTimeout(in usec) less than 5000 would be computed to
        // 0 milli seconds preempt timeout, and then after returning from
        // this method, we would end up in EXCLUSIVE mode
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *pNeedReload = false;
    result = pOsScheduler->setHeartbeatInterval(pProperties->watchdogTimeout);
    if ((currMode == ZES_SCHED_MODE_TIMEOUT) || (result != ZE_RESULT_SUCCESS)) {
        return result;
    }

    uint64_t timeout = (pProperties->watchdogTimeout) / 5;
    result = pOsScheduler->setPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    uint64_t timeslice = 0;
    result = pOsScheduler->setTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    return result;
}

ze_result_t SchedulerImp::setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) {
    if (pProperties->interval < minTimeoutInMicroSeconds) {
        // interval(in usec) less than 1000 would be computed to
        // 0 milli seconds interval.
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *pNeedReload = false;
    ze_result_t result = pOsScheduler->setPreemptTimeout(pProperties->yieldTimeout);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->setTimesliceDuration(pProperties->interval);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    uint64_t heartbeat = 2500 * (pProperties->interval);
    result = pOsScheduler->setHeartbeatInterval(heartbeat);
    return result;
}

ze_result_t SchedulerImp::schedulerGetProperties(zes_sched_properties_t *pProperties) {
    *pProperties = properties;
    return ZE_RESULT_SUCCESS;
}

void SchedulerImp::init() {
    pOsScheduler->getProperties(this->properties);
}

SchedulerImp::SchedulerImp(OsSysman *pOsSysman, zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines, ze_device_handle_t deviceHandle) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    pOsScheduler = OsScheduler::create(pOsSysman, engineType, listOfEngines,
                                       deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsScheduler);
    init();
};

SchedulerImp::~SchedulerImp() {
    if (nullptr != pOsScheduler) {
        delete pOsScheduler;
        pOsScheduler = nullptr;
    }
}

} // namespace L0

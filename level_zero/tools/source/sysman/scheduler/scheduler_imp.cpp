/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

constexpr uint64_t minTimeoutModeHeartbeat = 5000u;
constexpr uint64_t minTimeoutInMicroSeconds = 1000u;

ze_result_t SchedulerImp::getCurrentMode(zet_sched_mode_t *pMode) {
    uint64_t timeout = 0;
    uint64_t timeslice = 0;
    ze_result_t result = pOsScheduler->getPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->getTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    if (timeslice != 0) {
        *pMode = ZET_SCHED_MODE_TIMESLICE;
    } else {
        if (timeout > 0) {
            *pMode = ZET_SCHED_MODE_TIMEOUT;
        } else {
            *pMode = ZET_SCHED_MODE_EXCLUSIVE;
        }
    }
    return result;
}

ze_result_t SchedulerImp::getTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) {
    if (getDefaults) {
        pConfig->watchdogTimeout = defaultHeartbeat;
        return ZE_RESULT_SUCCESS;
    }

    uint64_t heartbeat = 0;
    ze_result_t result = pOsScheduler->getHeartbeatInterval(heartbeat);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pConfig->watchdogTimeout = heartbeat;

    return result;
}

ze_result_t SchedulerImp::getTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) {
    if (getDefaults) {
        pConfig->interval = defaultTimeslice;
        pConfig->yieldTimeout = defaultPreemptTimeout;
        return ZE_RESULT_SUCCESS;
    }

    uint64_t timeout = 0, timeslice = 0;
    ze_result_t result = pOsScheduler->getPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->getTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pConfig->interval = timeslice;
    pConfig->yieldTimeout = timeout;
    return result;
}

ze_result_t SchedulerImp::setTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    zet_sched_mode_t currMode;
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
    *pNeedReboot = false;
    result = pOsScheduler->setHeartbeatInterval(pProperties->watchdogTimeout);
    if ((currMode == ZET_SCHED_MODE_TIMEOUT) || (result != ZE_RESULT_SUCCESS)) {
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

ze_result_t SchedulerImp::setTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    if (pProperties->interval < minTimeoutInMicroSeconds) {
        // interval(in usec) less than 1000 would be computed to
        // 0 milli seconds interval.
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *pNeedReboot = false;
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

ze_result_t SchedulerImp::setExclusiveMode(ze_bool_t *pNeedReboot) {
    uint64_t timeslice = 0, timeout = 0, heartbeat = 0;
    *pNeedReboot = false;
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

ze_result_t SchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::init() {
    if (pOsScheduler == nullptr) {
        pOsScheduler = OsScheduler::create(pOsSysman);
    }
    if (nullptr == pOsScheduler) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    uint64_t timeout = 0;
    uint64_t timeslice = 0;
    uint64_t heartbeat = 0;
    ze_result_t result = pOsScheduler->getPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->getTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = pOsScheduler->getHeartbeatInterval(heartbeat);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    defaultPreemptTimeout = timeout;
    defaultTimeslice = timeslice;
    defaultHeartbeat = heartbeat;
    return result;
}

SchedulerImp::~SchedulerImp() {
    if (nullptr != pOsScheduler) {
        delete pOsScheduler;
        pOsScheduler = nullptr;
    }
}

} // namespace L0

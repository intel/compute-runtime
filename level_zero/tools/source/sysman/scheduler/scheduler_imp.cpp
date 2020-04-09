/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

ze_result_t SchedulerImp::getCurrentMode(zet_sched_mode_t *pMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::getTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::getTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::setTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::setTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::setExclusiveMode(ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void SchedulerImp::init() {
    if (pOsScheduler == nullptr) {
        pOsScheduler = OsScheduler::create(pOsSysman);
    }
    UNRECOVERABLE_IF(nullptr == pOsScheduler);
}

SchedulerImp::~SchedulerImp() {
    if (nullptr != pOsScheduler) {
        delete pOsScheduler;
    }
}

} // namespace L0

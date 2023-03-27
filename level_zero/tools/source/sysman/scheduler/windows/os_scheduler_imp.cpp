/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/scheduler/windows/os_scheduler_imp.h"

#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"

namespace L0 {

ze_result_t WddmSchedulerImp::getCurrentMode(zes_sched_mode_t *pMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setExclusiveMode(ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getProperties(zes_sched_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t OsScheduler::getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &listOfEngines, OsSysman *pOsSysman, ze_device_handle_t subdeviceHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::unique_ptr<OsScheduler> OsScheduler::create(
    OsSysman *pOsSysman, zes_engine_type_flag_t type, std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<WddmSchedulerImp> pWddmSchedulerImp = std::make_unique<WddmSchedulerImp>();
    return pWddmSchedulerImp;
}

} // namespace L0

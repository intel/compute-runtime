/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/scheduler/windows/os_scheduler_imp.h"

#include "sysman/scheduler/scheduler_imp.h"

namespace L0 {

ze_result_t WddmSchedulerImp::getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setPreemptTimeout(uint64_t timeout) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setTimesliceDuration(uint64_t timeslice) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setHeartbeatInterval(uint64_t heartbeat) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_bool_t WddmSchedulerImp::canControlScheduler() {
    return 0;
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

OsScheduler *OsScheduler::create(
    OsSysman *pOsSysman, zes_engine_type_flag_t type, std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice, uint32_t subdeviceId) {
    WddmSchedulerImp *pWddmSchedulerImp = new WddmSchedulerImp();
    return static_cast<OsScheduler *>(pWddmSchedulerImp);
}

} // namespace L0

/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

ze_result_t SchedulerImp::setExclusiveMode(ze_bool_t *pNeedReload) {
    return pOsScheduler->setExclusiveMode(pNeedReload);
}

ze_result_t SchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    return pOsScheduler->setComputeUnitDebugMode(pNeedReload);
}

ze_result_t SchedulerImp::getCurrentMode(zes_sched_mode_t *pMode) {
    return pOsScheduler->getCurrentMode(pMode);
}

ze_result_t SchedulerImp::getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) {
    return pOsScheduler->getTimeoutModeProperties(getDefaults, pConfig);
}

ze_result_t SchedulerImp::getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) {
    return pOsScheduler->getTimesliceModeProperties(getDefaults, pConfig);
}

ze_result_t SchedulerImp::setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) {
    return pOsScheduler->setTimeoutMode(pProperties, pNeedReload);
}

ze_result_t SchedulerImp::setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) {
    return pOsScheduler->setTimesliceMode(pProperties, pNeedReload);
}

ze_result_t SchedulerImp::schedulerGetProperties(zes_sched_properties_t *pProperties) {
    *pProperties = properties;
    return ZE_RESULT_SUCCESS;
}

void SchedulerImp::init() {
    pOsScheduler->getProperties(this->properties);
}

SchedulerImp::SchedulerImp(OsSysman *pOsSysman, zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines, ze_device_handle_t deviceHandle) {
    uint32_t subdeviceId = std::numeric_limits<uint32_t>::max();
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subdeviceId, onSubdevice, true);
    pOsScheduler = OsScheduler::create(pOsSysman, engineType, listOfEngines, onSubdevice, subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsScheduler);
    init();
};

SchedulerImp::~SchedulerImp() = default;

} // namespace L0

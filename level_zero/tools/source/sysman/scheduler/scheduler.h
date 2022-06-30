/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <map>
#include <string>
#include <vector>

struct _zes_sched_handle_t {
    virtual ~_zes_sched_handle_t() = default;
};

namespace L0 {
struct OsSysman;
class Scheduler : _zes_sched_handle_t {
  public:
    virtual ze_result_t schedulerGetProperties(zes_sched_properties_t *pProperties) = 0;
    virtual ze_result_t getCurrentMode(zes_sched_mode_t *pMode) = 0;
    virtual ze_result_t getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) = 0;
    virtual ze_result_t getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) = 0;
    virtual ze_result_t setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) = 0;
    virtual ze_result_t setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) = 0;
    virtual ze_result_t setExclusiveMode(ze_bool_t *pNeedReload) = 0;
    virtual ze_result_t setComputeUnitDebugMode(ze_bool_t *pNeedReload) = 0;

    static Scheduler *fromHandle(zes_sched_handle_t handle) {
        return static_cast<Scheduler *>(handle);
    }
    inline zes_sched_handle_t toHandle() { return this; }
    bool initSuccess = false;
};

struct SchedulerHandleContext : NEO::NonCopyableOrMovableClass {
    SchedulerHandleContext(OsSysman *pOsSysman);
    ~SchedulerHandleContext();
    void init(std::vector<ze_device_handle_t> &deviceHandles);
    ze_result_t schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler);

    OsSysman *pOsSysman = nullptr;
    std::vector<Scheduler *> handleList = {};
    ze_device_handle_t hCoreDevice = nullptr;

  private:
    void createHandle(zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines, ze_device_handle_t deviceHandle);
};

} // namespace L0

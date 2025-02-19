/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {
struct OsSysman;
class Scheduler : _zes_sched_handle_t {
  public:
    virtual ~Scheduler() = default;
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

struct SchedulerHandleContext : NEO::NonCopyableAndNonMovableClass {
    SchedulerHandleContext(OsSysman *pOsSysman);
    ~SchedulerHandleContext();
    void init(uint32_t subDeviceCount);
    ze_result_t schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler);

    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<Scheduler>> handleList = {};

  private:
    void createHandle(zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines, ze_bool_t onSubDevice, uint32_t subDeviceId);
    std::once_flag initSchedulerOnce;
};

} // namespace Sysman
} // namespace L0

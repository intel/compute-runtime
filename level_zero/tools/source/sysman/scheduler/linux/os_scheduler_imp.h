/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"

#include <string>

namespace L0 {
class SysfsAccess;
struct Device;

// Following below mappings of scheduler properties with sysfs nodes
// zes_sched_timeslice_properties_t.interval = timeslice_duration_ms
// zes_sched_timeslice_properties_t.yieldTimeout = preempt_timeout_ms
// zes_sched_timeout_properties_t. watchdogTimeout =  heartbeat_interval_ms
class LinuxSchedulerImp : public OsScheduler, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t setExclusiveMode(ze_bool_t *pNeedReload) override;
    ze_result_t getCurrentMode(zes_sched_mode_t *pMode) override;
    ze_result_t getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) override;
    ze_result_t getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) override;
    ze_result_t setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) override;
    ze_result_t setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) override;
    ze_result_t getProperties(zes_sched_properties_t &properties) override;
    ze_result_t setComputeUnitDebugMode(ze_bool_t *pNeedReload) override;
    LinuxSchedulerImp() = default;
    LinuxSchedulerImp(OsSysman *pOsSysman, zes_engine_type_flag_t type,
                      std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice, uint32_t subdeviceId);
    ~LinuxSchedulerImp() override = default;
    static const std::string engineDir;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    Device *pDevice = nullptr;
    zes_engine_type_flag_t engineType = ZES_ENGINE_TYPE_FLAG_OTHER;
    ze_bool_t onSubdevice = 0;
    uint32_t subdeviceId = 0;
    ze_result_t setExclusiveModeImp();
    ze_result_t updateComputeUnitDebugNode(uint64_t val);
    ze_result_t getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault);
    ze_result_t getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault);
    ze_result_t getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault);
    ze_result_t setPreemptTimeout(uint64_t timeout);
    ze_result_t setTimesliceDuration(uint64_t timeslice);
    ze_result_t setHeartbeatInterval(uint64_t heartbeat);
    ze_bool_t canControlScheduler();
    ze_result_t disableComputeUnitDebugMode(ze_bool_t *pNeedReload);
    bool isComputeUnitDebugModeEnabled();

  private:
    static const std::string preemptTimeoutMilliSecs;
    static const std::string defaultPreemptTimeouttMilliSecs;
    static const std::string timesliceDurationMilliSecs;
    static const std::string defaultTimesliceDurationMilliSecs;
    static const std::string heartbeatIntervalMilliSecs;
    static const std::string defaultHeartbeatIntervalMilliSecs;
    static const std::string enableEuDebug;
    std::vector<std::string> listOfEngines = {};
};

} // namespace L0

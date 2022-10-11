/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_neo.h"

#include "sysman/scheduler/scheduler_imp.h"

#include <string>

namespace L0 {
class SysfsAccess;
struct Device;

// Following below mappings of scheduler properties with sysfs nodes
// zes_sched_timeslice_properties_t.interval = timeslice_duration_ms
// zes_sched_timeslice_properties_t.yieldTimeout = preempt_timeout_ms
// zes_sched_timeout_properties_t. watchdogTimeout =  heartbeat_interval_ms
class LinuxSchedulerImp : public OsScheduler, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) override;
    ze_result_t getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault) override;
    ze_result_t getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault) override;
    ze_result_t setPreemptTimeout(uint64_t timeout) override;
    ze_result_t setTimesliceDuration(uint64_t timeslice) override;
    ze_result_t setHeartbeatInterval(uint64_t heartbeat) override;
    ze_bool_t canControlScheduler() override;
    ze_result_t getProperties(zes_sched_properties_t &properties) override;
    ze_result_t setComputeUnitDebugMode(ze_bool_t *pNeedReload) override;
    LinuxSchedulerImp() = default;
    LinuxSchedulerImp(OsSysman *pOsSysman, zes_engine_type_flag_t type,
                      std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice, uint32_t subdeviceId);
    ~LinuxSchedulerImp() override = default;
    static const std::string engineDir;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    Device *pDevice = nullptr;
    zes_engine_type_flag_t engineType = ZES_ENGINE_TYPE_FLAG_OTHER;
    ze_bool_t onSubdevice = 0;
    uint32_t subdeviceId = 0;

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

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "sysman/scheduler/scheduler_imp.h"

#include <string>

namespace L0 {
class SysfsAccess;

// Following below mappings of scheduler properties with sysfs nodes
// zet_sched_timeslice_properties_t.interval = timeslice_duration_ms
// zet_sched_timeslice_properties_t.yieldTimeout = preempt_timeout_ms
// zet_sched_timeout_properties_t. watchdogTimeout =  heartbeat_interval_ms
class LinuxSchedulerImp : public OsScheduler, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) override;
    ze_result_t getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault) override;
    ze_result_t getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault) override;
    ze_result_t setPreemptTimeout(uint64_t timeout) override;
    ze_result_t setTimesliceDuration(uint64_t timeslice) override;
    ze_result_t setHeartbeatInterval(uint64_t heartbeat) override;
    LinuxSchedulerImp() = default;
    LinuxSchedulerImp(OsSysman *pOsSysman);
    ~LinuxSchedulerImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    static const std::string preemptTimeoutMilliSecs;
    static const std::string defaultPreemptTimeouttMilliSecs;
    static const std::string timesliceDurationMilliSecs;
    static const std::string defaultTimesliceDurationMilliSecs;
    static const std::string heartbeatIntervalMilliSecs;
    static const std::string defaultHeartbeatIntervalMilliSecs;
};

} // namespace L0

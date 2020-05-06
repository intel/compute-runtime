/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "sysman/linux/os_sysman_imp.h"
#include "sysman/scheduler/scheduler_imp.h"

namespace L0 {

// Following below mappings of scheduler properties with sysfs nodes
// zet_sched_timeslice_properties_t.interval = timeslice_duration_ms
// zet_sched_timeslice_properties_t.yieldTimeout = preempt_timeout_ms
// zet_sched_timeout_properties_t. watchdogTimeout =  heartbeat_interval_ms
class LinuxSchedulerImp : public NEO::NonCopyableClass, public OsScheduler {
  public:
    ze_result_t getPreemptTimeout(uint64_t &timeout) override;
    ze_result_t getTimesliceDuration(uint64_t &timeslice) override;
    ze_result_t getHeartbeatInterval(uint64_t &heartbeat) override;
    ze_result_t setPreemptTimeout(uint64_t timeout) override;
    ze_result_t setTimesliceDuration(uint64_t timeslice) override;
    ze_result_t setHeartbeatInterval(uint64_t heartbeat) override;
    LinuxSchedulerImp() = default;
    LinuxSchedulerImp(OsSysman *pOsSysman);
    ~LinuxSchedulerImp() override = default;
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    static const std::string preemptTimeoutMilliSecs;
    static const std::string timesliceDurationMilliSecs;
    static const std::string heartbeatIntervalMilliSecs;
};

} // namespace L0

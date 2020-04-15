/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
    LinuxSchedulerImp(OsSysman *pOsSysman);
    ~LinuxSchedulerImp() override = default;

  private:
    SysfsAccess *pSysfsAccess;
    static const std::string preemptTimeoutMilliSecs;
    static const std::string timesliceDurationMilliSecs;
    static const std::string heartbeatIntervalMilliSecs;
};

const std::string LinuxSchedulerImp::preemptTimeoutMilliSecs("vcs0/preempt_timeout_ms");
const std::string LinuxSchedulerImp::timesliceDurationMilliSecs("vcs0/timeslice_duration_ms");
const std::string LinuxSchedulerImp::heartbeatIntervalMilliSecs("vcs0/heartbeat_interval_ms");
constexpr uint16_t milliSecsToMicroSecs = 1000;

ze_result_t LinuxSchedulerImp::getPreemptTimeout(uint64_t &timeout) {
    ze_result_t result = pSysfsAccess->read(preemptTimeoutMilliSecs, timeout);
    if (result == ZE_RESULT_SUCCESS) {
        timeout = timeout * milliSecsToMicroSecs;
    }
    return result;
}

ze_result_t LinuxSchedulerImp::getTimesliceDuration(uint64_t &timeslice) {
    ze_result_t result = pSysfsAccess->read(timesliceDurationMilliSecs, timeslice);
    if (result == ZE_RESULT_SUCCESS) {
        timeslice = timeslice * milliSecsToMicroSecs;
    }
    return result;
}

ze_result_t LinuxSchedulerImp::getHeartbeatInterval(uint64_t &heartbeat) {
    ze_result_t result = pSysfsAccess->read(heartbeatIntervalMilliSecs, heartbeat);
    if (result == ZE_RESULT_SUCCESS) {
        heartbeat = heartbeat * milliSecsToMicroSecs;
    }
    return result;
}

ze_result_t LinuxSchedulerImp::setPreemptTimeout(uint64_t timeout) {
    timeout = timeout / milliSecsToMicroSecs;
    return pSysfsAccess->write(preemptTimeoutMilliSecs, timeout);
}

ze_result_t LinuxSchedulerImp::setTimesliceDuration(uint64_t timeslice) {
    timeslice = timeslice / milliSecsToMicroSecs;
    return pSysfsAccess->write(timesliceDurationMilliSecs, timeslice);
}

ze_result_t LinuxSchedulerImp::setHeartbeatInterval(uint64_t heartbeat) {
    heartbeat = heartbeat / milliSecsToMicroSecs;
    return pSysfsAccess->write(heartbeatIntervalMilliSecs, heartbeat);
}

LinuxSchedulerImp::LinuxSchedulerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsScheduler *OsScheduler::create(OsSysman *pOsSysman) {
    LinuxSchedulerImp *pLinuxSchedulerImp = new LinuxSchedulerImp(pOsSysman);
    return static_cast<OsScheduler *>(pLinuxSchedulerImp);
}

} // namespace L0
/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

#include "sysman/linux/fs_access.h"

namespace L0 {
namespace ult {

const std::string preemptTimeoutMilliSecs("engine/rcs0/preempt_timeout_ms");
const std::string defaultPreemptTimeoutMilliSecs("engine/rcs0/.defaults/preempt_timeout_ms");
const std::string timesliceDurationMilliSecs("engine/rcs0/timeslice_duration_ms");
const std::string defaultTimesliceDurationMilliSecs("engine/rcs0/.defaults/timeslice_duration_ms");
const std::string heartbeatIntervalMilliSecs("engine/rcs0/heartbeat_interval_ms");
const std::string defaultHeartbeatIntervalMilliSecs("engine/rcs0/.defaults/heartbeat_interval_ms");
const std::string computeEngineDir("engine/rcs0");

class SchedulerSysfsAccess : public SysfsAccess {};

template <>
struct Mock<SchedulerSysfsAccess> : public SysfsAccess {
    uint64_t mockValPreemptTimeoutMilliSecs = 0;
    uint64_t mockValTimesliceDurationMilliSecs = 0;
    uint64_t mockValHeartbeatIntervalMilliSecs = 0;
    uint64_t mockValDefaultPreemptTimeoutMilliSecs = 0;
    uint64_t mockValDefaultTimesliceDurationMilliSecs = 0;
    uint64_t mockValDefaultHeartbeatIntervalMilliSecs = 0;

    ze_result_t getValForError(const std::string file, uint64_t &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getVal(const std::string file, uint64_t &val) {
        if (file.compare(preemptTimeoutMilliSecs) == 0) {
            val = mockValPreemptTimeoutMilliSecs;
        }
        if (file.compare(timesliceDurationMilliSecs) == 0) {
            val = mockValTimesliceDurationMilliSecs;
        }
        if (file.compare(heartbeatIntervalMilliSecs) == 0) {
            val = mockValHeartbeatIntervalMilliSecs;
        }
        if (file.compare(defaultPreemptTimeoutMilliSecs) == 0) {
            val = mockValDefaultPreemptTimeoutMilliSecs;
        }
        if (file.compare(defaultTimesliceDurationMilliSecs) == 0) {
            val = mockValDefaultTimesliceDurationMilliSecs;
        }
        if (file.compare(defaultHeartbeatIntervalMilliSecs) == 0) {
            val = mockValDefaultHeartbeatIntervalMilliSecs;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t setVal(const std::string file, const uint64_t val) {
        if (file.compare(preemptTimeoutMilliSecs) == 0) {
            mockValPreemptTimeoutMilliSecs = val;
        }
        if (file.compare(timesliceDurationMilliSecs) == 0) {
            mockValTimesliceDurationMilliSecs = val;
        }
        if (file.compare(heartbeatIntervalMilliSecs) == 0) {
            mockValHeartbeatIntervalMilliSecs = val;
        }
        if (file.compare(defaultPreemptTimeoutMilliSecs) == 0) {
            mockValDefaultPreemptTimeoutMilliSecs = val;
        }
        if (file.compare(defaultTimesliceDurationMilliSecs) == 0) {
            mockValDefaultTimesliceDurationMilliSecs = val;
        }
        if (file.compare(defaultHeartbeatIntervalMilliSecs) == 0) {
            mockValDefaultHeartbeatIntervalMilliSecs = val;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getCanReadStatus(const std::string file) {
        if (file.compare(computeEngineDir) == 0) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t getCanReadStatusReturnError(const std::string file) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    Mock<SchedulerSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const uint64_t val), (override));
    MOCK_METHOD(ze_result_t, canRead, (const std::string file), (override));
};

class PublicLinuxSchedulerImp : public L0::LinuxSchedulerImp {
  public:
    using LinuxSchedulerImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/linux/fs_access.h"
#include "sysman/sysman.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using ::testing::_;

namespace L0 {
namespace ult {

const std::string preemptTimeoutMilliSecs("engine/rcs0/preempt_timeout_ms");
const std::string timesliceDurationMilliSecs("engine/rcs0/timeslice_duration_ms");
const std::string heartbeatIntervalMilliSecs("engine/rcs0/heartbeat_interval_ms");

class SchedulerSysfsAccess : public SysfsAccess {};

template <>
struct Mock<SchedulerSysfsAccess> : public SysfsAccess {
    uint64_t mockValPreemptTimeoutMilliSecs = 0;
    uint64_t mockValTimesliceDurationMilliSecs = 0;
    uint64_t mockValHeartbeatIntervalMilliSecs = 0;

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
        return ZE_RESULT_SUCCESS;
    }

    Mock<SchedulerSysfsAccess>() = default;

    MOCK_METHOD2(read, ze_result_t(const std::string file, uint64_t &val));
    MOCK_METHOD2(write, ze_result_t(const std::string file, const uint64_t val));
};

class PublicLinuxSchedulerImp : public L0::LinuxSchedulerImp {
  public:
    using LinuxSchedulerImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

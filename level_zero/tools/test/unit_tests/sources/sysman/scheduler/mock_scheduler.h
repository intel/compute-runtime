/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/scheduler/os_scheduler.h"
#include "sysman/scheduler/scheduler_imp.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct Mock<OsScheduler> : public OsScheduler {
    uint64_t mockTimeout;
    uint64_t mockTimeslice;
    uint64_t mockHeartbeat;

    ze_result_t getMockTimeout(uint64_t &timeout) {
        timeout = mockTimeout;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t setMockTimeout(uint64_t timeout) {
        mockTimeout = timeout;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getMockTimeslice(uint64_t &timeslice) {
        timeslice = mockTimeslice;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t setMockTimeslice(uint64_t timeslice) {
        mockTimeslice = timeslice;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getMockHeartbeat(uint64_t &heartbeat) {
        heartbeat = mockHeartbeat;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t setMockHeartbeat(uint64_t heartbeat) {
        mockHeartbeat = heartbeat;
        return ZE_RESULT_SUCCESS;
    }
    Mock<OsScheduler>() = default;

    MOCK_METHOD1(getPreemptTimeout, ze_result_t(uint64_t &timeout));
    MOCK_METHOD1(getTimesliceDuration, ze_result_t(uint64_t &timeslice));
    MOCK_METHOD1(getHeartbeatInterval, ze_result_t(uint64_t &heartbeat));
    MOCK_METHOD1(setPreemptTimeout, ze_result_t(uint64_t timeout));
    MOCK_METHOD1(setTimesliceDuration, ze_result_t(uint64_t timeslice));
    MOCK_METHOD1(setHeartbeatInterval, ze_result_t(uint64_t heartbeat));
};

} // namespace ult
} // namespace L0

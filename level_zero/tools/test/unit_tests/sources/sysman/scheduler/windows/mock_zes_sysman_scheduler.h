/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/scheduler/os_scheduler.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

struct OsSchedulerMock : public OsScheduler {

    OsSchedulerMock() = default;

    ADDMETHOD_NOBASE(setComputeUnitDebugMode, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (ze_bool_t * pNeedReload));
    ADDMETHOD_NOBASE(getPreemptTimeout, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (uint64_t & timeout, ze_bool_t getDefault));
    ADDMETHOD_NOBASE(getTimesliceDuration, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (uint64_t & timeslice, ze_bool_t getDefault));
    ADDMETHOD_NOBASE(getHeartbeatInterval, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (uint64_t & heartbeat, ze_bool_t getDefault));
    ADDMETHOD_NOBASE(setPreemptTimeout, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (uint64_t timeout));
    ADDMETHOD_NOBASE(setTimesliceDuration, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (uint64_t timeslice));
    ADDMETHOD_NOBASE(setHeartbeatInterval, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (uint64_t heartbeat));
    ADDMETHOD_NOBASE(canControlScheduler, ze_bool_t, false, ());
    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (zes_sched_properties_t & properties));
};

} // namespace ult
} // namespace L0
/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "sysman/scheduler/scheduler_imp.h"

namespace L0 {

class WddmSchedulerImp : public OsScheduler {
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
};

} // namespace L0

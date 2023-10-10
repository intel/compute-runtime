/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/scheduler/sysman_scheduler_imp.h"

namespace L0 {
namespace Sysman {

class WddmSchedulerImp : public OsScheduler {
  public:
    ze_result_t getCurrentMode(zes_sched_mode_t *pMode) override;
    ze_result_t getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) override;
    ze_result_t getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) override;
    ze_result_t setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) override;
    ze_result_t setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) override;
    ze_result_t setExclusiveMode(ze_bool_t *pNeedReload) override;
    ze_result_t setComputeUnitDebugMode(ze_bool_t *pNeedReload) override;
    ze_result_t getProperties(zes_sched_properties_t &properties) override;
};

} // namespace Sysman
} // namespace L0

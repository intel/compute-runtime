/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

namespace L0 {

class Scheduler {
  public:
    virtual ~Scheduler(){};
    virtual ze_result_t getCurrentMode(zet_sched_mode_t *pMode) = 0;
    virtual ze_result_t getTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) = 0;
    virtual ze_result_t getTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) = 0;
    virtual ze_result_t setTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t setTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t setExclusiveMode(ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t setComputeUnitDebugMode(ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t init() = 0;
};

} // namespace L0
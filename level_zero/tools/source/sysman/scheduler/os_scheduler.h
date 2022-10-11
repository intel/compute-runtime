/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <map>
#include <string>
#include <vector>

namespace L0 {
struct OsSysman;
using namespace std;

class OsScheduler {
  public:
    virtual ze_result_t setComputeUnitDebugMode(ze_bool_t *pNeedReload) = 0;
    virtual ze_result_t getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) = 0;
    virtual ze_result_t getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault) = 0;
    virtual ze_result_t getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault) = 0;
    virtual ze_result_t setPreemptTimeout(uint64_t timeout) = 0;
    virtual ze_result_t setTimesliceDuration(uint64_t timeslice) = 0;
    virtual ze_result_t setHeartbeatInterval(uint64_t heartbeat) = 0;
    virtual ze_bool_t canControlScheduler() = 0;
    virtual ze_result_t getProperties(zes_sched_properties_t &properties) = 0;
    static OsScheduler *create(OsSysman *pOsSysman, zes_engine_type_flag_t engineType, std::vector<std::string> &listOfEngines,
                               ze_bool_t isSubdevice, uint32_t subdeviceId);
    static ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &listOfEngines,
                                                    OsSysman *pOsSysman, ze_device_handle_t subdeviceHandle);
    virtual ~OsScheduler() = default;
};

} // namespace L0

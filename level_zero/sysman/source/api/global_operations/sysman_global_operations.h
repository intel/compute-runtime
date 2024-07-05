/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class GlobalOperations {
  public:
    virtual ~GlobalOperations(){};
    virtual ze_result_t reset(ze_bool_t force) = 0;
    virtual ze_result_t deviceGetProperties(zes_device_properties_t *pProperties) = 0;
    virtual bool getDeviceInfoByUuid(zes_uuid_t uuid, ze_bool_t *onSubdevice, uint32_t *subdeviceId) = 0;
    virtual ze_result_t deviceGetSubDeviceProperties(uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps) = 0;
    virtual ze_result_t processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) = 0;
    virtual ze_result_t deviceGetState(zes_device_state_t *pState) = 0;
    virtual ze_result_t resetExt(zes_reset_properties_t *pProperties) = 0;

    virtual void init() = 0;
};

} // namespace Sysman
} // namespace L0

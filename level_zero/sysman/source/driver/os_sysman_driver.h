/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/zes_api.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {
namespace Sysman {

struct OsSysmanDriver {
    virtual ~OsSysmanDriver() {};

    virtual ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) = 0;
    virtual ze_result_t enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) = 0;
    static OsSysmanDriver *create();
};

} // namespace Sysman
} // namespace L0

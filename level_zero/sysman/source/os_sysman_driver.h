/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/zes_api.h"

namespace L0 {
namespace Sysman {

struct OsSysmanDriver {
    virtual ~OsSysmanDriver(){};

    virtual ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) = 0;
    static OsSysmanDriver *create();
};

} // namespace Sysman
} // namespace L0
/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

struct SysmanDriverHandle : BaseDriver, NEO::NonCopyableAndNonMovableClass {
    static SysmanDriverHandle *fromHandle(zes_driver_handle_t handle);
    inline zes_driver_handle_t toHandle() { return this; }

    static SysmanDriverHandle *create(NEO::ExecutionEnvironment &executionEnvironment, ze_result_t *returnValue);
    virtual ze_result_t getDeviceByUuid(zes_uuid_t uuid, zes_device_handle_t *phDevice, ze_bool_t *onSubdevice, uint32_t *subdeviceId) = 0;
    virtual ze_result_t getDevice(uint32_t *pCount, zes_device_handle_t *phDevices) = 0;
    virtual ze_result_t getExtensionProperties(uint32_t *pCount, zes_driver_extension_properties_t *pExtensionProperties) = 0;
    virtual ze_result_t sysmanEventsListen(uint32_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                           uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) = 0;
    virtual ze_result_t sysmanEventsListenEx(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                             uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) = 0;
};

static_assert(NEO::NonCopyableAndNonMovable<SysmanDriverHandle>);

} // namespace Sysman
} // namespace L0

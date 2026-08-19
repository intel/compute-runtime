/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/driver/os_sysman_driver.h"

namespace L0 {
namespace Sysman {

struct InfoLogHandleContext;

class WddmSysmanDriverImp : public OsSysmanDriver, NEO::NonCopyableAndNonMovableClass {
  public:
    WddmSysmanDriverImp() = default;
    ~WddmSysmanDriverImp() override;

    ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;
    ze_result_t driverEventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents) override;
    ze_result_t driverEventRegister(zes_event_type_flags_t events) override;
    ze_result_t enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) override;
    ze_result_t rescanDevices(SysmanDriverHandleImp *driverHandle, uint32_t *pCount, zes_device_handle_t *phDevices) override;

  private:
    InfoLogHandleContext *pInfoLogHandleContext = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<WddmSysmanDriverImp>);

} // namespace Sysman
} // namespace L0

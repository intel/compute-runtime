/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/driver/os_sysman_driver.h"

namespace L0 {
namespace Sysman {

class WddmSysmanDriverImp : public OsSysmanDriver {
  public:
    WddmSysmanDriverImp() = default;
    ~WddmSysmanDriverImp() override = default;

    ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;
};

} // namespace Sysman
} // namespace L0

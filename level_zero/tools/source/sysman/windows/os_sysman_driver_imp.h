/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman_driver.h"

namespace L0 {

class WddmSysmanDriverImp : public OsSysmanDriver {
  public:
    WddmSysmanDriverImp() = default;
    ~WddmSysmanDriverImp() override = default;

    ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;
};

} // namespace L0
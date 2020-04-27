/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include "os_sysman_device.h"
#include "sysman_device.h"

#include <vector>

namespace L0 {

class SysmanDeviceImp : public SysmanDevice {
  public:
    void init() override;
    ze_result_t deviceGetProperties(zet_sysman_properties_t *pProperties) override;
    ze_result_t reset() override;

    OsSysmanDevice *pOsSysmanDevice = nullptr;
    ze_device_handle_t hCoreDevice = {};

    SysmanDeviceImp() = default;
    SysmanDeviceImp(OsSysman *pOsSysman, ze_device_handle_t hCoreDevice) : hCoreDevice(hCoreDevice),
                                                                           pOsSysman(pOsSysman){};
    ~SysmanDeviceImp() override;

    // Don't allow copies of the SysmanDeviceImp object
    SysmanDeviceImp(const SysmanDeviceImp &obj) = delete;
    SysmanDeviceImp &operator=(const SysmanDeviceImp &obj) = delete;

  private:
    OsSysman *pOsSysman = nullptr;
    zet_sysman_properties_t sysmanProperties = {};
};

} // namespace L0

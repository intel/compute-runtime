/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include <vector>

namespace L0 {

SysmanDevice *SysmanDeviceHandleContext::init(ze_device_handle_t device) {
    auto isSysmanEnabled = getenv("ZES_ENABLE_SYSMAN");
    if ((isSysmanEnabled == nullptr) || (device == nullptr)) {
        return nullptr;
    }
    auto isSysmanEnabledAsInt = atoi(isSysmanEnabled);
    if (isSysmanEnabledAsInt == 1) {
        SysmanDeviceImp *sysman = new SysmanDeviceImp(device);
        UNRECOVERABLE_IF(!sysman);
        sysman->init();
        return sysman;
    }
    return nullptr;
}

void DeviceImp::setSysmanHandle(SysmanDevice *pSysman) {
    pSysmanDevice = pSysman;
}

SysmanDevice *DeviceImp::getSysmanHandle() {
    return pSysmanDevice;
}

} // namespace L0

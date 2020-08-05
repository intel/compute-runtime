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

SysmanDevice *SysmanDeviceHandleContext::init(ze_device_handle_t coreDevice) {
    SysmanDeviceImp *sysmanDevice = new SysmanDeviceImp(coreDevice);
    UNRECOVERABLE_IF(!sysmanDevice);
    sysmanDevice->init();
    return sysmanDevice;
}

void DeviceImp::setSysmanHandle(SysmanDevice *pSysmanDev) {
    pSysmanDevice = pSysmanDev;
}

SysmanDevice *DeviceImp::getSysmanHandle() {
    return pSysmanDevice;
}

} // namespace L0

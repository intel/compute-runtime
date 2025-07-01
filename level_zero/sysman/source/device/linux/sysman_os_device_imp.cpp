/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/device/os_sysman.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/device/sysman_os_device.h"

namespace L0 {
namespace Sysman {
struct OsSysman;

SysmanDevice *OsSysmanSurvivabilityDevice::createSurvivabilityDevice(std::unique_ptr<NEO::HwDeviceId> hwDeviceId) {
    SysmanDeviceImp *pSysmanDevice = new SysmanDeviceImp();
    DEBUG_BREAK_IF(!pSysmanDevice);
    ze_result_t result = pSysmanDevice->pOsSysman->initSurvivabilityMode(std::move(hwDeviceId));
    pSysmanDevice->isDeviceInSurvivabilityMode = true;
    if (result != ZE_RESULT_SUCCESS) {
        delete pSysmanDevice;
        pSysmanDevice = nullptr;
    }

    return pSysmanDevice;
}

} // namespace Sysman
} // namespace L0
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

ze_result_t DriverHandleImp::sysmanEventsListen(
    uint32_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    bool gotSysmanEvent = false;
    auto timeToExitLoop = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    do {
        for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
            gotSysmanEvent = L0::SysmanDevice::fromHandle(phDevices[devIndex])->deviceEventListen(pEvents[devIndex]);
            if (gotSysmanEvent) {
                *pNumDeviceEvents = 1;
                break;
            }
        }
        if (gotSysmanEvent) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Sleep for 10 milliseconds before next check of events
    } while ((std::chrono::steady_clock::now() <= timeToExitLoop));

    return ZE_RESULT_SUCCESS;
}

} // namespace L0

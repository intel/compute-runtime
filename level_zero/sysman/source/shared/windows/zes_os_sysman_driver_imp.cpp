/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/zes_os_sysman_driver_imp.h"

#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/sysman/source/api/info_log/sysman_info_log.h"
#include "level_zero/sysman/source/device/os_sysman.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmSysmanDriverImp::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    bool gotSysmanEvent = false;
    memset(pEvents, 0, count * sizeof(zes_event_type_flags_t));
    auto timeToExitLoop = L0::Sysman::SteadyClock::now() + std::chrono::duration<uint64_t, std::milli>(timeout);
    do {
        for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
            auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(phDevices[devIndex]);
            if (pSysmanDevice == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            gotSysmanEvent = pSysmanDevice->deviceEventListen(pEvents[devIndex], timeout);
            if (gotSysmanEvent) {
                *pNumDeviceEvents = 1;
                break;
            }
        }
        if (gotSysmanEvent) {
            break;
        }
        NEO::sleep(std::chrono::milliseconds(10)); // Sleep for 10 milliseconds before next check of events
    } while ((L0::Sysman::SteadyClock::now() <= timeToExitLoop));

    return ZE_RESULT_SUCCESS;
}

WddmSysmanDriverImp::~WddmSysmanDriverImp() {
    if (pInfoLogHandleContext != nullptr) {
        delete pInfoLogHandleContext;
        pInfoLogHandleContext = nullptr;
    }
}

ze_result_t WddmSysmanDriverImp::enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {
    if (pInfoLogHandleContext == nullptr) {
        pInfoLogHandleContext = new InfoLogHandleContext();
    }

    return pInfoLogHandleContext->infoLogGet(pCount, phInfoLogs);
}

OsSysmanDriver *OsSysmanDriver::create() {
    WddmSysmanDriverImp *pWddmSysmanDriverImp = new WddmSysmanDriverImp();
    return static_cast<OsSysmanDriver *>(pWddmSysmanDriverImp);
}

} // namespace Sysman
} // namespace L0

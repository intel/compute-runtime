/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/windows/os_sysman_driver_imp.h"

#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

namespace L0 {

ze_result_t WddmSysmanDriverImp::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    bool gotSysmanEvent = false;
    memset(pEvents, 0, count * sizeof(zes_event_type_flags_t));
    auto timeToExitLoop = L0::SteadyClock::now() + std::chrono::duration<uint64_t, std::milli>(timeout);
    do {
        for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
            gotSysmanEvent = L0::SysmanDevice::fromHandle(phDevices[devIndex])->deviceEventListen(pEvents[devIndex], timeout);
            if (gotSysmanEvent) {
                *pNumDeviceEvents = 1;
                break;
            }
        }
        if (gotSysmanEvent) {
            break;
        }
        NEO::sleep(std::chrono::milliseconds(10)); // Sleep for 10 milliseconds before next check of events
    } while ((L0::SteadyClock::now() <= timeToExitLoop));

    return ZE_RESULT_SUCCESS;
}

OsSysmanDriver *OsSysmanDriver::create() {
    WddmSysmanDriverImp *pWddmSysmanDriverImp = new WddmSysmanDriverImp();
    return static_cast<OsSysmanDriver *>(pWddmSysmanDriverImp);
}

} // namespace L0

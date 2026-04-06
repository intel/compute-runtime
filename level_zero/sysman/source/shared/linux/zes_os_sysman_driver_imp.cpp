/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_imp.h"
#include "level_zero/sysman/source/device/sysman_device.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxSysmanDriverImp::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    ze_result_t res = pLinuxEventsUtil->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
    if (ZE_RESULT_SUCCESS != res) {
        return res;
    }

    // handle runtime survivability event
    for (uint32_t index = 0; index < count; index++) {
        if (pEvents[index] & ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED) {
            auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(phDevices[index]);
            pSysmanDevice->isDeviceInSurvivabilityMode = true;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Device %d got Survivability event\n", index);
        }
    }

    return ZE_RESULT_SUCCESS;
}

void LinuxSysmanDriverImp::eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice) {
    pLinuxEventsUtil->eventRegister(events, pSysmanDevice);
}

L0::Sysman::UdevLib *LinuxSysmanDriverImp::getUdevLibHandle() {
    if (pUdevLib == nullptr) {
        pUdevLib = UdevLib::create();
    }
    return pUdevLib;
}

LinuxSysmanDriverImp::LinuxSysmanDriverImp() {
    pLinuxEventsUtil = new LinuxEventsUtil(this);
}

LinuxSysmanDriverImp::~LinuxSysmanDriverImp() {
    if (nullptr != pUdevLib) {
        delete pUdevLib;
        pUdevLib = nullptr;
    }

    if (nullptr != pLinuxEventsUtil) {
        delete pLinuxEventsUtil;
        pLinuxEventsUtil = nullptr;
    }
}

OsSysmanDriver *OsSysmanDriver::create() {
    LinuxSysmanDriverImp *pLinuxSysmanDriverImp = new LinuxSysmanDriverImp();
    DEBUG_BREAK_IF(nullptr == pLinuxSysmanDriverImp);
    return static_cast<OsSysmanDriver *>(pLinuxSysmanDriverImp);
}

} // namespace Sysman
} // namespace L0

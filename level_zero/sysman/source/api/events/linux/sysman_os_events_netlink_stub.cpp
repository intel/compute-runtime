/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_imp.h"
#include "level_zero/sysman/source/api/events/linux/sysman_os_events_netlink.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"

namespace L0 {
namespace Sysman {

// Stub implementation when libnl is not available
ze_result_t netlinkInitialize(LinuxSysmanDriverImp *pDriver, DrmNlApi *&pDrmNl, SysmanDeviceImp *pSysmanDevice) {
    pDrmNl = nullptr;
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool netlinkHandleEvents(DrmNlApi *pDrmNl,
                         zes_event_type_flags_t *pEvents,
                         uint32_t count,
                         zes_device_handle_t *phDevices,
                         const std::vector<zes_event_type_flags_t> &registeredEvents) {
    return false;
}

bool netlinkProcessRasEvent(const DrmRasEvent &netlinkEvent,
                            zes_event_type_flags_t *pEvents,
                            uint32_t count,
                            zes_device_handle_t *phDevices,
                            const std::vector<zes_event_type_flags_t> &registeredEvents) {
    return false;
}

int netlinkGetSocketFd(DrmNlApi *pDrmNl) {
    return -1;
}

bool isNetlinkSubscribed(DrmNlApi *pDrmNl) {
    return false;
}

} // namespace Sysman
} // namespace L0

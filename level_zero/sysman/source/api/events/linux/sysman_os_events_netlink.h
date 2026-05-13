/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {
namespace Sysman {

class DrmNlApi;
class LinuxSysmanDriverImp;
struct DrmRasEvent;
struct SysmanDeviceImp;

ze_result_t netlinkInitialize(LinuxSysmanDriverImp *pDriver, DrmNlApi *&pDrmNl, SysmanDeviceImp *pSysmanDevice);
bool netlinkHandleEvents(DrmNlApi *pDrmNl,
                         zes_event_type_flags_t *pEvents,
                         uint32_t count,
                         zes_device_handle_t *phDevices,
                         const std::vector<zes_event_type_flags_t> &registeredEvents);
bool netlinkProcessRasEvent(const DrmRasEvent &netlinkEvent,
                            zes_event_type_flags_t *pEvents,
                            uint32_t count,
                            zes_device_handle_t *phDevices,
                            const std::vector<zes_event_type_flags_t> &registeredEvents);
int netlinkGetSocketFd(DrmNlApi *pDrmNl);
bool isNetlinkSubscribed(DrmNlApi *pDrmNl);

} // namespace Sysman
} // namespace L0

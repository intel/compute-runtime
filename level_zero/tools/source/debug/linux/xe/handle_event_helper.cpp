/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"

namespace L0 {

void DebugSessionLinuxXe::additionalEvents(drm_xe_eudebug_event *event) {
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: UNHANDLED %u flags = %u len = %lu\n", (uint16_t)event->type, (uint16_t)event->flags, (uint32_t)event->len);
}

bool DebugSessionLinuxXe::eventTypeIsAttention(uint16_t eventType) {
    return (eventType == DRM_XE_EUDEBUG_EVENT_EU_ATTENTION);
}

} // namespace L0
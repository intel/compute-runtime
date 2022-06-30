/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"

namespace NEO {

OsContext *OsContext::create(OSInterface *osInterface, uint32_t contextId, const EngineDescriptor &engineDescriptor) {
    if (osInterface) {
        if (osInterface->getDriverModel()->getDriverModelType() == DriverModelType::DRM) {
            return OsContextLinux::create(osInterface, contextId, engineDescriptor);
        } else {
            return OsContextWin::create(osInterface, contextId, engineDescriptor);
        }
    }
    return OsContextLinux::create(osInterface, contextId, engineDescriptor);
}

} // namespace NEO

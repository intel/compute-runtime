/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

OsContext *OsContext::create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                             EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice) {
    return OsContextLinux::create(osInterface, contextId, deviceBitfield, typeUsage, preemptionMode, rootDevice);
}

} // namespace NEO

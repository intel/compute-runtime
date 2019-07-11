/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/os_context_linux.h"

#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/os_context.h"

namespace NEO {

OsContext *OsContext::create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                             aub_stream::EngineType engineType, PreemptionMode preemptionMode, bool lowPriority) {
    if (osInterface) {
        return new OsContextLinux(*osInterface->get()->getDrm(), contextId, deviceBitfield, engineType, preemptionMode, lowPriority);
    }
    return new OsContext(contextId, deviceBitfield, engineType, preemptionMode, lowPriority);
}

OsContextLinux::OsContextLinux(Drm &drm, uint32_t contextId, DeviceBitfield deviceBitfield,
                               aub_stream::EngineType engineType, PreemptionMode preemptionMode, bool lowPriority)
    : OsContext(contextId, deviceBitfield, engineType, preemptionMode, lowPriority), drm(drm) {
    for (auto deviceIndex = 0u; deviceIndex < deviceBitfield.size(); deviceIndex++) {
        if (deviceBitfield.test(deviceIndex)) {
            auto drmContextId = drm.createDrmContext();
            if (drm.isPreemptionSupported() && lowPriority) {
                drm.setLowPriorityContextParam(drmContextId);
            }
            this->engineFlag = drm.bindDrmContext(drmContextId, deviceIndex, engineType);
            this->drmContextIds.push_back(drmContextId);
        }
    }
}

OsContextLinux::~OsContextLinux() {
    for (auto drmContextId : drmContextIds) {
        drm.destroyDrmContext(drmContextId);
    }
}
} // namespace NEO

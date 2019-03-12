/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/os_context_linux.h"

#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/os_context.h"

namespace OCLRT {

OsContext *OsContext::create(OSInterface *osInterface, uint32_t contextId, uint32_t deviceBitfield,
                             EngineInstanceT engineType, PreemptionMode preemptionMode) {
    if (osInterface) {
        return new OsContextLinux(*osInterface->get()->getDrm(), contextId, deviceBitfield, engineType, preemptionMode);
    }
    return new OsContext(contextId, deviceBitfield, engineType, preemptionMode);
}

OsContextLinux::OsContextLinux(Drm &drm, uint32_t contextId, uint32_t deviceBitfield,
                               EngineInstanceT engineType, PreemptionMode preemptionMode)
    : OsContext(contextId, deviceBitfield, engineType, preemptionMode), drm(drm) {

    engineFlag = DrmEngineMapper::engineNodeMap(engineType.type);
    this->drmContextId = drm.createDrmContext();
    if (drm.isPreemptionSupported() &&
        engineType.type == lowPriorityGpgpuEngine.type &&
        engineType.id == lowPriorityGpgpuEngine.id) {
        drm.setLowPriorityContextParam(this->drmContextId);
    }
}

OsContextLinux::~OsContextLinux() {
    drm.destroyDrmContext(drmContextId);
}
} // namespace OCLRT

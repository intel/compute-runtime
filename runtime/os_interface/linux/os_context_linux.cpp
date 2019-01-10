/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"

namespace OCLRT {

OsContext::OsContext(OSInterface *osInterface, uint32_t contextId, EngineInstanceT engineType, PreemptionMode preemptionMode)
    : contextId(contextId), engineType(engineType) {
    if (osInterface) {
        osContextImpl = std::make_unique<OsContextLinux>(*osInterface->get()->getDrm(), engineType);
    }
}

OsContext::~OsContext() = default;

OsContextLinux::OsContextImpl(Drm &drm, EngineInstanceT engineType) : drm(drm) {
    engineFlag = DrmEngineMapper::engineNodeMap(engineType.type);
    this->drmContextId = drm.createDrmContext();
    if (drm.isPreemptionSupported() &&
        engineType.type == lowPriorityGpgpuEngine.type &&
        engineType.id == lowPriorityGpgpuEngine.id) {
        drm.setLowPriorityContextParam(this->drmContextId);
    }
}

OsContextLinux::~OsContextImpl() {
    drm.destroyDrmContext(drmContextId);
}
} // namespace OCLRT

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"

namespace OCLRT {

OsContext::OsContext(OSInterface *osInterface, uint32_t contextId) : contextId(contextId) {
    if (osInterface) {
        osContextImpl = std::make_unique<OsContextLinux>(*osInterface->get()->getDrm());
    }
}

OsContext::~OsContext() = default;

OsContextLinux::OsContextImpl(Drm &drm) : drm(drm) {}
} // namespace OCLRT

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_context.h"

namespace OCLRT {
class OsContext::OsContextImpl {};
OsContext::OsContext(OSInterface *osInterface, uint32_t contextId) : contextId(contextId) {
    osContextImpl = std::make_unique<OsContext::OsContextImpl>();
}

OsContext::~OsContext() = default;
} // namespace OCLRT

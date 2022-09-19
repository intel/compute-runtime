/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"

namespace NEO {
class MockOsContext : public OsContext {
  public:
    using OsContext::checkDirectSubmissionSupportsEngine;
    using OsContext::debuggableContext;
    using OsContext::engineType;
    using OsContext::engineUsage;
    using OsContext::getDeviceBitfield;

    MockOsContext(uint32_t contextId, const EngineDescriptor &engineDescriptorHelper)
        : OsContext(contextId, engineDescriptorHelper) {}
};
static_assert(sizeof(OsContext) == sizeof(MockOsContext));

} // namespace NEO
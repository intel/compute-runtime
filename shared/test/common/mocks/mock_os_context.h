/*
 * Copyright (C) 2019-2025 Intel Corporation
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
    using OsContext::contextInitialized;
    using OsContext::debuggableContext;
    using OsContext::engineType;
    using OsContext::engineUsage;
    using OsContext::getDeviceBitfield;

    MockOsContext(uint32_t contextId, const EngineDescriptor &engineDescriptorHelper)
        : OsContext(0, contextId, engineDescriptorHelper) {}
};
static_assert(sizeof(OsContext) == sizeof(MockOsContext));

class OsContextMock : public OsContext {
  public:
    using OsContext::checkDirectSubmissionSupportsEngine;
    using OsContext::contextInitialized;
    using OsContext::debuggableContext;
    using OsContext::engineType;
    using OsContext::engineUsage;
    using OsContext::getDeviceBitfield;

    using OsContext::getEngineType;
    using OsContext::getUmdPowerHintMax;
    using OsContext::isDirectSubmissionAvailable;
    using OsContext::isDirectSubmissionSupported;
    using OsContext::OsContext;
    using OsContext::setDefaultContext;
    using OsContext::setUmdPowerHintValue;

    OsContextMock(uint32_t contextId, const EngineDescriptor &engineDescriptorHelper)
        : OsContext(0, contextId, engineDescriptorHelper) {}

    uint64_t getOfflineDumpContextId(uint32_t deviceIndex) const override { return offlineDumpCtxId; };

    bool isDirectSubmissionSupported() const override {
        if (callBaseIsDirectSubmissionSupported) {
            return OsContext::isDirectSubmissionSupported();
        }
        return mockDirectSubmissionSupported;
    }

    uint64_t offlineDumpCtxId = 0;

    bool callBaseIsDirectSubmissionSupported = true;
    bool mockDirectSubmissionSupported = false;
};

} // namespace NEO
/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/os_context_linux.h"

namespace NEO {
class MockOsContextLinux : public OsContextLinux {
  public:
    using OsContextLinux::drmContextIds;
    using OsContextLinux::drmVmIds;
    using OsContextLinux::fenceVal;
    using OsContextLinux::ovLoaded;
    using OsContextLinux::pagingFence;

    MockOsContextLinux(Drm &drm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
        : OsContextLinux(drm, rootDeviceIndex, contextId, engineDescriptor) {}
};
static_assert(sizeof(OsContextLinux) == sizeof(MockOsContextLinux));

} // namespace NEO
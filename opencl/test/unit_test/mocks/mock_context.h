/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/context/context.h"
#include "opencl/source/sharings/sharing_factory.h"

#include <memory>

namespace NEO {
class MockContext : public Context {
  public:
    using Context::contextType;
    using Context::driverDiagnostics;
    using Context::memoryManager;
    using Context::preferD3dSharedResources;
    using Context::sharingFunctions;
    using Context::svmAllocsManager;
    MockContext(ClDevice *device, bool noSpecialQueue = false);
    MockContext(
        void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
        void *data);
    MockContext();
    ~MockContext() override;

    void clearSharingFunctions();
    void setSharingFunctions(SharingFunctions *sharingFunctions);
    void releaseSharingFunctions(SharingType sharing);
    void resetSharingFunctions(SharingType sharing);
    void registerSharingWithId(SharingFunctions *sharing, SharingType sharingId);

  private:
    ClDevice *device = nullptr;
};
} // namespace NEO

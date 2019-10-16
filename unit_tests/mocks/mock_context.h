/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/context/context.h"
#include "runtime/sharings/sharing_factory.h"

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
    MockContext(Device *device, bool noSpecialQueue = false);
    MockContext(
        void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
        void *data);
    MockContext();
    ~MockContext();

    void clearSharingFunctions();
    void setSharingFunctions(SharingFunctions *sharingFunctions);
    void releaseSharingFunctions(SharingType sharing);
    void resetSharingFunctions(SharingType sharing);
    void registerSharingWithId(SharingFunctions *sharing, SharingType sharingId);

  private:
    Device *device;
};
} // namespace NEO

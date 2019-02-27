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

namespace OCLRT {
class MockContext : public Context {
  public:
    using Context::sharingFunctions;

    MockContext(Device *device, bool noSpecialQueue = false);
    MockContext(
        void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
        void *data);
    MockContext();
    ~MockContext();

    void setMemoryManager(MemoryManager *mm) {
        memoryManager = mm;
    }

    void clearSharingFunctions();
    void setSharingFunctions(SharingFunctions *sharingFunctions);
    void setContextType(ContextType contextType);
    void releaseSharingFunctions(SharingType sharing);
    void resetSharingFunctions(SharingType sharing);
    void registerSharingWithId(SharingFunctions *sharing, SharingType sharingId);

    cl_bool peekPreferD3dSharedResources() { return preferD3dSharedResources; }

    void forcePreferD3dSharedResources(cl_bool value) { preferD3dSharedResources = value; }
    DriverDiagnostics *getDriverDiagnostics() { return this->driverDiagnostics; }

  private:
    Device *device;
};
} // namespace OCLRT

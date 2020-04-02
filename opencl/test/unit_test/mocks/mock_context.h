/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/context/context.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include <memory>

namespace NEO {

class AsyncEventsHandler;

class MockContext : public Context {
  public:
    using Context::contextType;
    using Context::driverDiagnostics;
    using Context::memoryManager;
    using Context::preferD3dSharedResources;
    using Context::sharingFunctions;
    using Context::svmAllocsManager;
    MockContext(ClDevice *pDevice, bool noSpecialQueue = false);
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
    std::unique_ptr<AsyncEventsHandler> &getAsyncEventsHandlerUniquePtr();

  protected:
    void initializeWithDevices(const ClDeviceVector &devices, bool noSpecialQueue);

  private:
    ClDevice *pDevice = nullptr;
};

struct MockDefaultContext : MockContext {
    MockDefaultContext();

    UltClDeviceFactory ultClDeviceFactory{2, 0};
    MockClDevice *pRootDevice0;
    MockClDevice *pRootDevice1;
};

struct MockSpecializedContext : MockContext {
    MockSpecializedContext();

    UltClDeviceFactory ultClDeviceFactory{1, 2};
    MockClDevice *pRootDevice;
    ClDevice *pSubDevice0 = nullptr;
    ClDevice *pSubDevice1 = nullptr;
};

struct MockUnrestrictiveContext : MockContext {
    MockUnrestrictiveContext();

    UltClDeviceFactory ultClDeviceFactory{1, 2};
    MockClDevice *pRootDevice;
    ClDevice *pSubDevice0 = nullptr;
    ClDevice *pSubDevice1 = nullptr;
};

} // namespace NEO

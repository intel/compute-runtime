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
    using Context::deviceBitfields;
    using Context::driverDiagnostics;
    using Context::maxRootDeviceIndex;
    using Context::memoryManager;
    using Context::preferD3dSharedResources;
    using Context::resolvesRequiredInKernels;
    using Context::rootDeviceIndices;
    using Context::setupContextType;
    using Context::sharingFunctions;
    using Context::specialQueues;
    using Context::svmAllocsManager;

    MockContext(ClDevice *pDevice, bool noSpecialQueue = false);
    MockContext(const ClDeviceVector &clDeviceVector, bool noSpecialQueue = true);
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
    void initializeWithDevices(const ClDeviceVector &devices, bool noSpecialQueue);

  private:
    ClDevice *pDevice = nullptr;
};

struct MockDefaultContext : MockContext {
    MockDefaultContext();

    UltClDeviceFactory ultClDeviceFactory{3, 0};
    MockClDevice *pRootDevice0;
    MockClDevice *pRootDevice1;
    MockClDevice *pRootDevice2;
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

struct MockUnrestrictiveContextMultiGPU : MockContext {
    MockUnrestrictiveContextMultiGPU();

    UltClDeviceFactory ultClDeviceFactory{2, 2};
    MockClDevice *pRootDevice0;
    ClDevice *pSubDevice00 = nullptr;
    ClDevice *pSubDevice01 = nullptr;
    MockClDevice *pRootDevice1;
    ClDevice *pSubDevice10 = nullptr;
    ClDevice *pSubDevice11 = nullptr;
};

} // namespace NEO

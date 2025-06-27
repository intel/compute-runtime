/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/blit_helper.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/source/context/context.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include <memory>

namespace NEO {

class AsyncEventsHandler;
class OsContext;
class ClDevice;
class ClDeviceVector;
class CommandStreamReceiver;
class MockClDevice;
class SharingFunctions;

class MockContext : public Context {
  public:
    using Context::contextType;
    using Context::deviceBitfields;
    using Context::devices;
    using Context::driverDiagnostics;
    using Context::getUsmDevicePoolParams;
    using Context::getUsmHostPoolParams;
    using Context::maxRootDeviceIndex;
    using Context::memoryManager;
    using Context::preferD3dSharedResources;
    using Context::resolvesRequiredInKernels;
    using Context::rootDeviceIndices;
    using Context::setupContextType;
    using Context::sharingFunctions;
    using Context::smallBufferPoolAllocator;
    using Context::specialQueues;
    using Context::svmAllocsManager;
    using Context::usmPoolInitialized;
    using Context::UsmPoolParams;

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

    class MockBufferPoolAllocator : public BufferPoolAllocator {
      public:
        using BufferPoolAllocator::bufferPools;
        using BufferPoolAllocator::calculateMaxPoolCount;
        using BufferPoolAllocator::isAggregatedSmallBuffersEnabled;
        using BufferPoolAllocator::params;
    };

  private:
    ClDevice *pDevice = nullptr;
};

struct MockDefaultContext : MockContext {
    MockDefaultContext();
    MockDefaultContext(bool initSpecialQueues);

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

class BcsMockContext : public MockContext {
  public:
    BcsMockContext(ClDevice *device);
    ~BcsMockContext() override;

    std::unique_ptr<OsContext> bcsOsContext;
    std::unique_ptr<CommandStreamReceiver> bcsCsr;
    VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
        &BlitHelperFunctions::blitMemoryToAllocation};
};
} // namespace NEO

/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"

namespace L0 {
namespace ult {

using MemoryIPCTests = MemoryExportImportTest;

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleWithNotKnownPointerThenInvalidArgumentIsReturned) {

    uint32_t value = 0;

    ze_ipc_mem_handle_t ipcHandle;
    ze_result_t result = context->getIpcMemHandle(&value, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleWithDeviceAllocationThenIpcHandleIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleWithDeviceAllocationAndCallToPutIpcHandleThenIpcHandleIsReturnedAndReleased) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleWithDeviceAllocationAndCallToMemFreeThenIPCHandleIsClosed) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleWithDeviceAllocationAndCallToGetFdFromIpcHandleThenIPCHandleReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_external_memory_export_desc_t exportDesc = {};
    exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceAllocDesc.pNext = &exportDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceAllocDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_external_memory_export_fd_t exportFd = {};
    exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    ze_memory_allocation_properties_t allocProps = {};
    allocProps.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    allocProps.pNext = &exportFd;
    ze_device_handle_t deviceHandle;
    context->memPropTest = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->getMemAllocProperties(ptr, &allocProps, &deviceHandle));

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t readIpcHandle = 0;
    result = context->getFdFromIpcHandle(ipcHandle, &readIpcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(static_cast<int>(readIpcHandle), exportFd.fd);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleFromFdThenValidIpcHandleReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_external_memory_export_desc_t exportDesc = {};
    exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceAllocDesc.pNext = &exportDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceAllocDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_external_memory_export_fd_t exportFd = {};
    exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    ze_memory_allocation_properties_t allocProps = {};
    allocProps.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    allocProps.pNext = &exportFd;
    ze_device_handle_t deviceHandle;
    context->memPropTest = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->getMemAllocProperties(ptr, &allocProps, &deviceHandle));

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcHandleFromFd(exportFd.fd, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    EXPECT_EQ(static_cast<int>(ipcData.handle), exportFd.fd);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleFromFdThenUsageInOpenIPCHandleSucceeds) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_external_memory_export_desc_t exportDesc = {};
    exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceAllocDesc.pNext = &exportDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceAllocDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_external_memory_export_fd_t exportFd = {};
    exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    ze_memory_allocation_properties_t allocProps = {};
    allocProps.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    allocProps.pNext = &exportFd;
    ze_device_handle_t deviceHandle;
    context->memPropTest = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->getMemAllocProperties(ptr, &allocProps, &deviceHandle));

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcHandleFromFd(exportFd.fd, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    EXPECT_EQ(static_cast<int>(ipcData.handle), exportFd.fd);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandle(deviceHandle, ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleFromFdWithInvalidFdThenErrorReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_external_memory_export_desc_t exportDesc = {};
    exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceAllocDesc.pNext = &exportDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceAllocDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcHandleFromFd(0u, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct ContextOpaqueHandleMock : public L0::ContextImp {
    ContextOpaqueHandleMock(DriverHandleGetFdMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }

    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override {
        ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
        if (ZE_RESULT_SUCCESS == res) {
            driverHandle->allocationMap.first = *ptr;
            driverHandle->allocationMap.second = driverHandle->mockFd;
        }
        return res;
    }

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override {
        if (memPropTest) {
            pMemAllocProperties->type = ZE_MEMORY_TYPE_DEVICE;
            ze_external_memory_export_fd_t *exportFd = static_cast<ze_external_memory_export_fd_t *>(pMemAllocProperties->pNext);
            exportFd->fd = driverHandle->allocationMap.second;
            if (phDevice != nullptr) {
                *phDevice = device->toHandle();
            }
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    // Helper function to manually add IPC handle tracking with specified opaque flag
    void addIpcHandleTracking(uint64_t handle, bool useOpaqueHandle) {
        auto lock = this->driverHandle->lockIPCHandleMap();
        auto *tracking = new IpcHandleTracking();
        tracking->handle = handle;
        tracking->refcnt = 1;
        tracking->opaqueIpcHandle = useOpaqueHandle;

        if (useOpaqueHandle) {
            tracking->opaqueData.handle.fd = static_cast<int>(handle);
            tracking->opaqueData.memoryType = static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory);
            tracking->opaqueData.processId = 12345;
            tracking->opaqueData.poolOffset = 0;
            tracking->opaqueData.type = IpcHandleType::fdHandle;
        } else {
            tracking->ipcData.handle = handle;
            tracking->ipcData.type = static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory);
            tracking->ipcData.poolOffset = 0;
        }

        this->driverHandle->getIPCHandleMap()[handle] = tracking;
    }

    DriverHandleGetFdMock *driverHandle = nullptr;
    L0::Device *device = nullptr;
    bool memPropTest = false;
};

struct MemoryIPCOpaqueHandleTests : public ::testing::Test {
    void SetUp() override {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleGetFdMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextOpaqueHandleMock>(driverHandle.get());
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        context->device = device;
    }

    void TearDown() override {
        // Clean up any remaining IPC handle tracking
        auto &ipcMap = driverHandle->getIPCHandleMap();
        for (auto it = ipcMap.begin(); it != ipcMap.end(); ++it) {
            delete it->second;
        }
        ipcMap.clear();

        context.reset(nullptr);
        driverHandle.reset(nullptr);
    }

    std::unique_ptr<DriverHandleGetFdMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextOpaqueHandleMock> context;
};

TEST_F(MemoryIPCOpaqueHandleTests,
       givenCallToGetIpcHandleFromFdWithOpaqueHandleFlagSetThenValidOpaqueIpcHandleReturned) {
    uint64_t testHandle = 57;

    // Add IPC handle tracking with opaque flag set to true
    context->addIpcHandleTracking(testHandle, true);

    ze_ipc_mem_handle_t ipcHandle;
    ze_result_t result = context->getIpcHandleFromFd(testHandle, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Verify opaque data is returned
    IpcOpaqueMemoryData &ipcOpaqueData = *reinterpret_cast<IpcOpaqueMemoryData *>(ipcHandle.data);
    EXPECT_EQ(static_cast<int>(ipcOpaqueData.handle.fd), static_cast<int>(testHandle));
    EXPECT_EQ(ipcOpaqueData.memoryType, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(ipcOpaqueData.processId, 12345u);
    EXPECT_EQ(ipcOpaqueData.poolOffset, 0u);
    EXPECT_EQ(ipcOpaqueData.type, IpcHandleType::fdHandle);
}

TEST_F(MemoryIPCOpaqueHandleTests,
       givenCallToGetIpcHandleFromFdWithOpaqueHandleFlagUnsetThenRegularIpcHandleReturned) {
    uint64_t testHandle = 42;

    // Add IPC handle tracking with opaque flag set to false
    context->addIpcHandleTracking(testHandle, false);

    ze_ipc_mem_handle_t ipcHandle;
    ze_result_t result = context->getIpcHandleFromFd(testHandle, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Verify regular IPC data is returned
    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    EXPECT_EQ(static_cast<int>(ipcData.handle), static_cast<int>(testHandle));
    EXPECT_EQ(ipcData.type, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(ipcData.poolOffset, 0u);
}

TEST_F(MemoryIPCOpaqueHandleTests,
       givenCallToGetIpcHandleFromFdWithNonExistentHandleThenErrorReturned) {
    uint64_t nonExistentHandle = 999;

    // Don't add any IPC handle tracking for this handle

    ze_ipc_mem_handle_t ipcHandle;
    ze_result_t result = context->getIpcHandleFromFd(nonExistentHandle, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
}

TEST_F(MemoryIPCOpaqueHandleTests,
       givenCallToGetIpcHandleFromFdWithMultipleHandlesHavingDifferentOpaqueFlags) {
    uint64_t opaqueHandle = 100;
    uint64_t regularHandle = 200;

    // Add one handle with opaque flag set and another with opaque flag unset
    context->addIpcHandleTracking(opaqueHandle, true);
    context->addIpcHandleTracking(regularHandle, false);

    // Test opaque handle
    ze_ipc_mem_handle_t opaqueIpcHandle;
    ze_result_t result = context->getIpcHandleFromFd(opaqueHandle, &opaqueIpcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    IpcOpaqueMemoryData &opaqueData = *reinterpret_cast<IpcOpaqueMemoryData *>(opaqueIpcHandle.data);
    EXPECT_EQ(static_cast<int>(opaqueData.handle.fd), static_cast<int>(opaqueHandle));
    EXPECT_EQ(opaqueData.processId, 12345u);

    // Test regular handle
    ze_ipc_mem_handle_t regularIpcHandle;
    result = context->getIpcHandleFromFd(regularHandle, &regularIpcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    IpcMemoryData &regularData = *reinterpret_cast<IpcMemoryData *>(regularIpcHandle.data);
    EXPECT_EQ(static_cast<int>(regularData.handle), static_cast<int>(regularHandle));
    EXPECT_EQ(regularData.type, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory));
}

TEST_F(MemoryIPCOpaqueHandleTests,
       givenCallToGetIpcHandleFromFdWithZeroHandleAndOpaqueFlag) {
    uint64_t zeroHandle = 0;

    // Add IPC handle tracking with handle 0 and opaque flag set
    context->addIpcHandleTracking(zeroHandle, true);

    ze_ipc_mem_handle_t ipcHandle;
    ze_result_t result = context->getIpcHandleFromFd(zeroHandle, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Verify opaque data is returned with handle 0
    IpcOpaqueMemoryData &ipcOpaqueData = *reinterpret_cast<IpcOpaqueMemoryData *>(ipcHandle.data);
    EXPECT_EQ(static_cast<int>(ipcOpaqueData.handle.fd), 0);
    EXPECT_EQ(ipcOpaqueData.memoryType, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory));
}

TEST_F(MemoryIPCTests,
       givenCallToGetIpcHandleWithDeviceAllocationAndCallToGetFdFromIpcHandleWithInvalidHandleThenErrorReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_external_memory_export_desc_t exportDesc = {};
    exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceAllocDesc.pNext = &exportDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceAllocDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_external_memory_export_fd_t exportFd = {};
    exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    ze_memory_allocation_properties_t allocProps = {};
    allocProps.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    allocProps.pNext = &exportFd;
    ze_device_handle_t deviceHandle;
    context->memPropTest = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->getMemAllocProperties(ptr, &allocProps, &deviceHandle));

    ze_ipc_mem_handle_t ipcHandle;
    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    ipcData.handle = 256;
    uint64_t readIpcHandle = 256;
    result = context->getFdFromIpcHandle(ipcHandle, &readIpcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenMultipleCallsToGetIpcHandleWithDeviceAllocationAndCallsToPutIpcHandleThenIpcHandleIsReturnedAndReleased) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenMultipleCallsToGetIpcHandleWithDeviceAllocationAndOneCallToPutIpcHandleThenIpcHandleIsReturnedAndReleased) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());
}

TEST_F(MemoryIPCTests,
       givenCallsToGetIpcHandleWithDeviceAllocationWithDifferentContextsThenIpcHandleClosed) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    std::unique_ptr<ContextFdMock> contextInvalid;
    contextInvalid = std::make_unique<ContextFdMock>(driverHandle.get());
    EXPECT_NE(contextInvalid, nullptr);
    contextInvalid->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    contextInvalid->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    contextInvalid->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = contextInvalid->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, driverHandle->getIPCHandleMap().size());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       whenCallingGetIpcHandleWithHostAllocationThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       whenCallingOpenIpcHandleWithIpcHandleThenDeviceAllocationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ipcPtr, ptr);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       whenCallingOpenIpcHandleWithIpcHandleAndUsingContextThenDeviceAllocationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ipcPtr, ptr);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       givenCallingGetIpcHandleWithDeviceAllocationAndUsingContextThenIpcHandleIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIPCTests,
       whenCallingOpenIpcHandleWithIncorrectHandleThenInvalidArgumentIsReturned) {
    ze_ipc_mem_handle_t ipcHandle = {};
    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    ze_result_t res = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

struct MemoryGetIpcHandleTest : public ::testing::Test {
    void SetUp() override {

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleGetIpcHandleMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextGetIpcHandleMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
    }

    std::unique_ptr<DriverHandleGetIpcHandleMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextGetIpcHandleMock> context;
};

TEST_F(MemoryGetIpcHandleTest,
       whenCallingOpenIpcHandleWithIpcHandleThenFdHandleIsCorrectlyRead) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ipcPtr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryOpenIpcHandleTest,
       givenCallToOpenIpcMemHandleItIsSuccessfullyOpenedAndClosed) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(ipcPtr, nullptr);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedImportDescriptorAndSupportedFlagThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_device_mem_alloc_desc_t importDeviceDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedImportDesc.fd = extendedProperties.fd;
    importDeviceDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToHostAllocWithExtendedImportDescriptorAndSupportedFlagThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    hostDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    result = context->getMemAllocProperties(ptr, &memoryProperties, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_host_mem_alloc_desc_t importHostDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedImportDesc.fd = extendedProperties.fd;
    importHostDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *importedPtr = nullptr;
    result = context->allocHostMem(&importHostDesc,
                                   size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedImportDescriptorAndOpaqueFDFlagThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_device_mem_alloc_desc_t importDeviceDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedImportDesc.fd = extendedProperties.fd;
    importDeviceDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToHostAllocWithExtendedImportDescriptorAndOpaqueFDFlagThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    hostDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    result = context->getMemAllocProperties(ptr, &memoryProperties, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_host_mem_alloc_desc_t importHostDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedImportDesc.fd = extendedProperties.fd;
    importHostDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *importedPtr = nullptr;
    result = context->allocHostMem(&importHostDesc,
                                   size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToHostAllocWithExtendedImportDescriptorAndWDDMDriverTypeThenFailureIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    hostDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    result = context->getMemAllocProperties(ptr, &memoryProperties, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_host_mem_alloc_desc_t importHostDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedImportDesc.fd = extendedProperties.fd;
    importHostDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    void *importedPtr = nullptr;
    result = context->allocHostMem(&importHostDesc,
                                   size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedImportDescriptorAndWDDMDriverTypeThenFailureIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_device_mem_alloc_desc_t importDeviceDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedImportDesc.fd = extendedProperties.fd;
    importDeviceDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportWSLTest,
       givenCallToDeviceAllocWithExtendedImportDescriptorAndNTHandleWithWDDMThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = true;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_device_mem_alloc_desc_t importDeviceDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedImportDesc.fd = extendedProperties.fd;
    importDeviceDesc.pNext = &extendedImportDesc;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedImportDescriptorAndNonSupportedFlagThenInvalidArgumentIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_device_mem_alloc_desc_t importDeviceDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedImportDesc.fd = extendedProperties.fd;
    importDeviceDesc.pNext = &extendedImportDesc;

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, importedPtr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

class DrmIpcPidfdFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        auto &rootDeviceEnvironment{*neoDevice->executionEnvironment->rootDeviceEnvironments[0]};
        drmMock = new DrmMock(rootDeviceEnvironment);

        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(drmMock));
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }

    DrmMock *drmMock = nullptr;
};

using MemoryDrmIpcPidfdTests = Test<DrmIpcPidfdFixture>;
TEST_F(MemoryDrmIpcPidfdTests,
       givenCallToGetIpcHandleWithDeviceAllocationWithPidFdSocketMethodAndMockDrmThenIpcHandleIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryDrmIpcPidfdTests,
       givenCallToGetIpcHandleWithDeviceAllocationWithPidFdSocketMethodFalseAndMockDrmThenIpcHandleIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(0);
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0

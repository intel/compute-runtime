/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"

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
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelDRM>(512));

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
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelDRM>(512));

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
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleGetIpcHandleMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextGetIpcHandleMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
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
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelDRM>(512));

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
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelDRM>(512));

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
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelDRM>(512));

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"

namespace L0 {
namespace ult {

using MemoryIPCTests = MemoryExportImportWinHandleTest;

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

    std::unique_ptr<ContextHandleMock> contextInvalid;
    contextInvalid = std::make_unique<ContextHandleMock>(driverHandle.get());
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
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleGetWinHandleMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextHandleMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
    }

    std::unique_ptr<DriverHandleGetWinHandleMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextHandleMock> context;
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

} // namespace ult
} // namespace L0
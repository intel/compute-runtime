/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

class ContextWhiteboxForWindowsIpcTest : public ::L0::ContextImp {
  public:
    ContextWhiteboxForWindowsIpcTest(L0::DriverHandle *driverHandle) : L0::ContextImp(driverHandle) {}

    using ::L0::ContextImp::setIPCHandleData;
};

using DriverHandleIpcSocketWindowsTest = Test<DeviceFixture>;

TEST_F(DriverHandleIpcSocketWindowsTest, givenIpcSocketServerDeleterWhenCalledWithNullptrThenNoOp) {
    NEO::IpcSocketServerDeleter deleter;
    deleter(nullptr);
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenWindowsDriverHandleWhenInitializeIpcSocketServerCalledThenReturnsFalse) {
    bool result = driverHandle->initializeIpcSocketServer();
    EXPECT_FALSE(result);
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenWindowsDriverHandleWhenShutdownIpcSocketServerCalledThenNoOp) {
    driverHandle->shutdownIpcSocketServer();
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenWindowsDriverHandleWhenRegisterIpcHandleWithServerCalledThenReturnsFalse) {
    bool result = driverHandle->registerIpcHandleWithServer(12345, 42);
    EXPECT_FALSE(result);
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenWindowsDriverHandleWhenUnregisterIpcHandleWithServerCalledThenReturnsFalse) {
    bool result = driverHandle->unregisterIpcHandleWithServer(12345);
    EXPECT_FALSE(result);
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenWindowsDriverHandleWhenGetIpcSocketServerPathCalledThenReturnsEmptyString) {
    std::string result = driverHandle->getIpcSocketServerPath();
    EXPECT_TRUE(result.empty());
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenOpaqueHandleWithFdTypeAndSocketFallbackEnabledOnWindowsThenSocketServerInitializationFailsAndHandleStillAdded) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableIpcSocketFallback.set(1);

    // Verify that initializeIpcSocketServer returns false on Windows
    EXPECT_FALSE(driverHandle->initializeIpcSocketServer());

    ContextWhiteboxForWindowsIpcTest contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 12345;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x1000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    // Socket server is an optional fallback mechanism - handle tracking is primary and must always succeed
    // Even if socket server init fails, the handle should still be added to the map
    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    EXPECT_EQ(&mockAllocation, handleIterator->second->alloc);
    EXPECT_EQ(1u, handleIterator->second->refcnt);
    EXPECT_EQ(ptrAddress, handleIterator->second->ptr);
    EXPECT_EQ(handle, handleIterator->second->handle);

    int fdValue = 0;
    memcpy(&fdValue, &handleIterator->second->opaqueData.handle.fd, sizeof(fdValue));
    EXPECT_EQ(handle, static_cast<uint64_t>(fdValue));

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenOpaqueHandleWithNtHandleTypeAndSocketFallbackEnabledThenSocketServerCodePathIsSkipped) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableIpcSocketFallback.set(1);

    ContextWhiteboxForWindowsIpcTest contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 11111;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x3000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    // ntHandle type skips the socket server code path entirely
    // Socket fallback is only for fdHandle type
    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::ntHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    uint64_t reservedValue = 0;
    memcpy(&reservedValue, &handleIterator->second->opaqueData.handle.reserved, sizeof(reservedValue));
    EXPECT_EQ(handle, reservedValue);

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenOpaqueHandleWithFdTypeAndSocketFallbackDisabledThenSocketServerCodePathIsSkipped) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableIpcSocketFallback.set(0);

    ContextWhiteboxForWindowsIpcTest contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 22222;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x4000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    // Socket fallback disabled - socket server code path is skipped
    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    int fdValue = 0;
    memcpy(&fdValue, &handleIterator->second->opaqueData.handle.fd, sizeof(fdValue));
    EXPECT_EQ(handle, static_cast<uint64_t>(fdValue));

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

class MockDriverHandleForIpcSocket : public L0::DriverHandle {
  public:
    bool initializeIpcSocketServer() override {
        initializeIpcSocketServerCalled++;
        return initializeIpcSocketServerResult;
    }

    bool registerIpcHandleWithServer(uint64_t handleId, int fd) override {
        registerIpcHandleWithServerCalled++;
        lastRegisteredHandleId = handleId;
        lastRegisteredFd = fd;
        return registerIpcHandleWithServerResult;
    }

    uint32_t initializeIpcSocketServerCalled = 0;
    bool initializeIpcSocketServerResult = false;

    uint32_t registerIpcHandleWithServerCalled = 0;
    bool registerIpcHandleWithServerResult = false;
    uint64_t lastRegisteredHandleId = 0;
    int lastRegisteredFd = 0;
};

class ContextWhiteboxForMockDriverHandle : public ::L0::ContextImp {
  public:
    ContextWhiteboxForMockDriverHandle(L0::DriverHandle *driverHandle) : L0::ContextImp(driverHandle) {}

    using ::L0::ContextImp::setIPCHandleData;
};

TEST_F(DriverHandleIpcSocketWindowsTest, givenOpaqueHandleWithFdTypeAndSocketServerInitSucceedsAndRegisterSucceedsThenHandleIsRegistered) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableIpcSocketFallback.set(1);
    NEO::debugManager.flags.ForceIpcSocketFallback.set(1);

    MockDriverHandleForIpcSocket mockDriverHandle;
    mockDriverHandle.initializeIpcSocketServerResult = true;
    mockDriverHandle.registerIpcHandleWithServerResult = true;

    ContextWhiteboxForMockDriverHandle contextWhitebox(&mockDriverHandle);
    contextWhitebox.settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    contextWhitebox.settings.handleType = L0::IpcHandleType::fdHandle;

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 33333;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x5000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    EXPECT_EQ(1u, mockDriverHandle.initializeIpcSocketServerCalled);
    EXPECT_EQ(1u, mockDriverHandle.registerIpcHandleWithServerCalled);
    EXPECT_EQ(handle, mockDriverHandle.lastRegisteredHandleId);
    EXPECT_EQ(static_cast<int>(handle), mockDriverHandle.lastRegisteredFd);

    auto &ipcHandleMap = mockDriverHandle.getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    delete handleIterator->second;
    mockDriverHandle.getIPCHandleMap().clear();
}

TEST_F(DriverHandleIpcSocketWindowsTest, givenOpaqueHandleWithFdTypeAndSocketServerInitSucceedsAndRegisterFailsThenHandleStillAdded) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableIpcSocketFallback.set(1);
    NEO::debugManager.flags.ForceIpcSocketFallback.set(1);

    MockDriverHandleForIpcSocket mockDriverHandle;
    mockDriverHandle.initializeIpcSocketServerResult = true;
    mockDriverHandle.registerIpcHandleWithServerResult = false;

    ContextWhiteboxForMockDriverHandle contextWhitebox(&mockDriverHandle);
    contextWhitebox.settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    contextWhitebox.settings.handleType = L0::IpcHandleType::fdHandle;

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 44444;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x6000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    EXPECT_EQ(1u, mockDriverHandle.initializeIpcSocketServerCalled);
    EXPECT_EQ(1u, mockDriverHandle.registerIpcHandleWithServerCalled);

    auto &ipcHandleMap = mockDriverHandle.getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    delete handleIterator->second;
    mockDriverHandle.getIPCHandleMap().clear();
}

} // namespace ult
} // namespace L0

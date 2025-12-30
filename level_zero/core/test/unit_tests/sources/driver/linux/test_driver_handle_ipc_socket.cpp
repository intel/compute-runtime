/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

class MockIpcSocketServer : public NEO::IpcSocketServer {
  public:
    bool initializeReturnValue = true;
    bool registerHandleReturnValue = true;
    bool unregisterHandleReturnValue = true;
    std::string mockSocketPath = "test_socket_path";
    bool running = false;

    bool initialize() override {
        running = initializeReturnValue;
        return initializeReturnValue;
    }

    void shutdown() override {
        running = false;
    }

    bool isRunning() const override {
        return running;
    }

    std::string getSocketPath() const override {
        return mockSocketPath;
    }

    bool registerHandle(uint64_t handleId, int fd) override {
        return registerHandleReturnValue;
    }

    bool unregisterHandle(uint64_t handleId) override {
        return unregisterHandleReturnValue;
    }
};

class DriverHandleIpcSocketTest : public Test<DeviceFixture> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWhenInitializeIpcSocketServerSucceedsThenReturnTrue) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;

    driverHandle->ipcSocketServer.reset(mockServer);

    bool result = driverHandle->initializeIpcSocketServer();
    EXPECT_TRUE(result);
    EXPECT_TRUE(mockServer->isRunning());

    result = driverHandle->initializeIpcSocketServer();
    EXPECT_TRUE(result);

    driverHandle->shutdownIpcSocketServer();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWhenInitializeIpcSocketServerFailsThenReturnFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> socketBackup(&NEO::SysCalls::sysCallsSocket);
    NEO::SysCalls::sysCallsSocket = [](int, int, int) -> int { return -1; };

    bool result = driverHandle->initializeIpcSocketServer();
    EXPECT_FALSE(result);
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithIpcServerWhenRegisterIpcHandleSucceedsThenReturnTrue) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;
    mockServer->registerHandleReturnValue = true;

    driverHandle->ipcSocketServer.reset(mockServer);

    bool result = driverHandle->registerIpcHandleWithServer(12345, 42);
    EXPECT_TRUE(result);

    driverHandle->shutdownIpcSocketServer();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNoIpcServerWhenRegisterIpcHandleThenReturnFalse) {
    bool result = driverHandle->registerIpcHandleWithServer(12345, 42);
    EXPECT_FALSE(result);
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithIpcServerWhenUnregisterIpcHandleSucceedsThenReturnTrue) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;
    mockServer->unregisterHandleReturnValue = true;

    driverHandle->ipcSocketServer.reset(mockServer);

    bool result = driverHandle->unregisterIpcHandleWithServer(12345);
    EXPECT_TRUE(result);

    driverHandle->shutdownIpcSocketServer();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNoIpcServerWhenUnregisterIpcHandleThenReturnFalse) {
    bool result = driverHandle->unregisterIpcHandleWithServer(12345);
    EXPECT_FALSE(result);
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithIpcServerWhenGetIpcSocketServerPathThenReturnPath) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;
    mockServer->mockSocketPath = "neo_ipc_test_path";

    driverHandle->ipcSocketServer.reset(mockServer);

    std::string path = driverHandle->getIpcSocketServerPath();
    EXPECT_FALSE(path.empty());
    EXPECT_NE(path.find("neo_ipc_"), std::string::npos);

    driverHandle->shutdownIpcSocketServer();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNoIpcServerWhenGetIpcSocketServerPathThenReturnEmptyString) {
    std::string path = driverHandle->getIpcSocketServerPath();
    EXPECT_TRUE(path.empty());
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWhenShutdownIpcSocketServerCalledMultipleTimesThenNoError) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;

    driverHandle->ipcSocketServer.reset(mockServer);

    driverHandle->shutdownIpcSocketServer();
    driverHandle->shutdownIpcSocketServer();

    EXPECT_TRUE(driverHandle->getIpcSocketServerPath().empty());
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWhenShutdownCalledWithoutInitializeThenNoError) {
    driverHandle->shutdownIpcSocketServer();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithShutdownServerWhenRegisterHandleThenReturnFalse) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;

    driverHandle->ipcSocketServer.reset(mockServer);
    driverHandle->shutdownIpcSocketServer();

    EXPECT_FALSE(driverHandle->registerIpcHandleWithServer(12345, 42));
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithShutdownServerWhenUnregisterHandleThenReturnFalse) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = true;

    driverHandle->ipcSocketServer.reset(mockServer);
    driverHandle->shutdownIpcSocketServer();

    EXPECT_FALSE(driverHandle->unregisterIpcHandleWithServer(12345));
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNonRunningServerWhenRegisterIpcHandleThenReturnFalse) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = false;
    driverHandle->ipcSocketServer.reset(mockServer);

    bool result = driverHandle->registerIpcHandleWithServer(12345, 42);
    EXPECT_FALSE(result);

    driverHandle->ipcSocketServer.reset();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNonRunningServerWhenUnregisterIpcHandleThenReturnFalse) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = false;
    driverHandle->ipcSocketServer.reset(mockServer);

    bool result = driverHandle->unregisterIpcHandleWithServer(12345);
    EXPECT_FALSE(result);

    driverHandle->ipcSocketServer.reset();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNonRunningServerWhenGetIpcSocketServerPathThenReturnEmptyString) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = false;
    driverHandle->ipcSocketServer.reset(mockServer);

    std::string path = driverHandle->getIpcSocketServerPath();
    EXPECT_TRUE(path.empty());

    driverHandle->ipcSocketServer.reset();
}

TEST_F(DriverHandleIpcSocketTest, givenDriverHandleWithNonRunningServerWhenInitializeIpcSocketServerThenReInitializes) {
    auto mockServer = new MockIpcSocketServer();
    mockServer->running = false;
    mockServer->initializeReturnValue = true;
    driverHandle->ipcSocketServer.reset(mockServer);

    bool result = driverHandle->initializeIpcSocketServer();
    EXPECT_TRUE(result);
    EXPECT_TRUE(driverHandle->ipcSocketServer->isRunning());

    driverHandle->shutdownIpcSocketServer();
}

} // namespace ult
} // namespace L0

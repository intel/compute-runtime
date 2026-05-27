/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include <atomic>
#include <thread>

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

using DriverHandleIpcSocketMtTest = Test<DeviceFixture>;

TEST_F(DriverHandleIpcSocketMtTest, givenConcurrentRegisterAndUnregisterIpcHandleWhenCalledFromMultipleThreadsThenNoRace) {
    auto *mockServer = new MockIpcSocketServer();
    mockServer->running = true;
    driverHandle->ipcSocketServer.reset(mockServer);

    constexpr int iterations = 500;
    std::atomic<bool> go{false};

    auto registerer = std::thread([&] {
        while (!go.load(std::memory_order_acquire)) {
        }
        for (int i = 0; i < iterations; ++i) {
            driverHandle->registerIpcHandleWithServer(static_cast<uint64_t>(i + 1), i + 1);
        }
    });

    auto unregisterer = std::thread([&] {
        while (!go.load(std::memory_order_acquire)) {
        }
        for (int i = 0; i < iterations; ++i) {
            driverHandle->unregisterIpcHandleWithServer(static_cast<uint64_t>(i + 1));
        }
    });

    go.store(true, std::memory_order_release);
    registerer.join();
    unregisterer.join();

    driverHandle->shutdownIpcSocketServer();
}

} // namespace ult
} // namespace L0

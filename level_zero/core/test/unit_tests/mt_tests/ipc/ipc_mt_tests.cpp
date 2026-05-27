/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include <atomic>
#include <thread>
#include <vector>

namespace L0 {
namespace ult {

class ContextWhiteboxForIpcMtTest : public ::L0::Context {
  public:
    ContextWhiteboxForIpcMtTest(L0::DriverHandle *driverHandle) : ::L0::Context(driverHandle) {}
    using ::L0::Context::setIPCHandleData;
};

using ContextIpcSocketMtTest = Test<DeviceFixture>;

TEST_F(ContextIpcSocketMtTest, givenConcurrentSetIpcHandleDataWhenCalledFromMultipleThreadsThenNoRace) {
    constexpr int numThreads = 4;
    constexpr int handlesPerThread = 100;
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;

    ContextWhiteboxForIpcMtTest contextWhitebox(driverHandle.get());
    std::vector<NEO::MockGraphicsAllocation> allocs(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) {
            }
            for (int i = 0; i < handlesPerThread; ++i) {
                uint64_t h = static_cast<uint64_t>(t * handlesPerThread + i + 1);
                L0::IpcOpaqueMemoryData opaqueIpcData = {};
                contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(
                    &allocs[t], h, opaqueIpcData, h * 0x1000u,
                    static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory),
                    nullptr, L0::IpcHandleType::ntHandle, nullptr);
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(static_cast<size_t>(numThreads * handlesPerThread),
              driverHandle->getIPCHandleMap().size());

    auto &map = driverHandle->getIPCHandleMap();
    for (auto &[k, v] : map) {
        delete v;
    }
    map.clear();
}

} // namespace ult
} // namespace L0

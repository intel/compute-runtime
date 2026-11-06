/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <mutex>
#include <thread>

using namespace NEO;

namespace {
class HostPtrManagerLockProbingMemoryManager : public MockMemoryManager {
  public:
    using MockMemoryManager::MockMemoryManager;

    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override {
        auto &allocationsMutex = static_cast<MockHostPtrManager *>(hostPtrManager.get())->allocationsMutex;
        std::thread probeThread([this, &allocationsMutex] {
            std::unique_lock<std::recursive_mutex> probeLock(allocationsMutex, std::try_to_lock);
            hostPtrManagerLockedDuringCleanup = !probeLock.owns_lock();
        });
        probeThread.join();
        MockMemoryManager::cleanOsHandles(handleStorage, rootDeviceIndex);
    }

    bool hostPtrManagerLockedDuringCleanup = false;
};
} // namespace

TEST(HostPtrManagerMtTest, givenUsmHostPtrFragmentsEnabledAndAllocationBackedByFragmentsWhenItIsFreedThenFragmentsAreCleanedWhileHostPtrManagerIsLocked) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmHostPtrFragments.set(1);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    HostPtrManagerLockProbingMemoryManager memoryManager(executionEnvironment);
    void *cpuPtr = reinterpret_cast<void *>(0x100040);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0u, false, MemoryConstants::pageSize, DeviceBitfield(1)}, cpuPtr);
    ASSERT_NE(nullptr, allocation);
    ASSERT_NE(0u, allocation->fragmentsStorage.fragmentCount);

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_TRUE(memoryManager.hostPtrManagerLockedDuringCleanup);
}

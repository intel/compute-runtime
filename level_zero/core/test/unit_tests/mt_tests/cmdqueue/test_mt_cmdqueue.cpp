/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <thread>

namespace L0 {
namespace ult {

struct SVMAllocsManagerMock : public NEO::SVMAllocsManager {
    using SVMAllocsManager::mtxForIndirectAccess;
    SVMAllocsManagerMock(NEO::MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void makeIndirectAllocationsResident(NEO::CommandStreamReceiver &commandStreamReceiver, TaskCountType taskCount) override {
        makeIndirectAllocationsResidentCalledTimes++;
    }
    void addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                    NEO::ResidencyContainer &residencyContainer,
                                                    uint32_t requestedTypesMask) override {
        addInternalAllocationsToResidencyContainerCalledTimes++;
        passedContainer = residencyContainer.data();
    }
    uint32_t makeIndirectAllocationsResidentCalledTimes = 0;
    uint32_t addInternalAllocationsToResidencyContainerCalledTimes = 0;
    NEO::GraphicsAllocation **passedContainer = nullptr;
};

using CommandQueueCreateMt = Test<DeviceFixture>;

TEST_F(CommandQueueCreateMt, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmiPackDisabeldThenSVMAllocsMtxIsLocked) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto prevSvmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::unique_lock<std::mutex> lock;
    auto mockSvmAllocsManager = std::make_unique<SVMAllocsManagerMock>(device->getDriverHandle()->getMemoryManager());
    reinterpret_cast<WhiteBox<::L0::DriverHandle> *>(device->getDriverHandle())->svmAllocsManager = mockSvmAllocsManager.get();

    commandQueue->handleIndirectAllocationResidency({true, true, true}, lock, false);
    std::thread th([&] {
        EXPECT_FALSE(mockSvmAllocsManager->mtxForIndirectAccess.try_lock());
    });
    th.join();
    reinterpret_cast<WhiteBox<::L0::DriverHandle> *>(device->getDriverHandle())->svmAllocsManager = prevSvmAllocsManager;
    lock.unlock();
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0

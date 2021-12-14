/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

class WddmMemManagerFixture {
  public:
    struct FrontWindowMemManagerMock : public MockWddmMemoryManager {
        using MemoryManager::allocate32BitGraphicsMemoryImpl;
        FrontWindowMemManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : MockWddmMemoryManager(executionEnvironment) {}
    };

    void SetUp() {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
        auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
        wddm->callBaseMapGpuVa = false;
        memManager = std::unique_ptr<FrontWindowMemManagerMock>(new FrontWindowMemManagerMock(*executionEnvironment));
    }
    void TearDown() {
    }
    std::unique_ptr<FrontWindowMemManagerMock> memManager;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

using WddmFrontWindowPoolAllocatorTests = Test<WddmMemManagerFixture>;

TEST_F(WddmFrontWindowPoolAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenWddmAllocate32BitGraphicsMemoryThenAllocateAtHeapBegining) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation = memManager->allocate32BitGraphicsMemoryImpl(allocData, false);
    EXPECT_EQ(allocation->getGpuBaseAddress(), GmmHelper::canonize(allocation->getGpuAddress()));
    memManager->freeGraphicsMemory(allocation);
}
} // namespace NEO
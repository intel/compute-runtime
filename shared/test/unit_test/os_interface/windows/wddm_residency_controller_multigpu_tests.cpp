/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>
#include <vector>

using namespace NEO;

struct WddmResidencyControllerMultiGpuSharedAllocationTest : ::testing::Test {
    const uint32_t osContextId1 = 0u;
    const uint32_t osContextId2 = 1u;

    void SetUp() override {
        // Set up WDDM for root device 0
        wddm0 = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
        wddm0->init();
        gdi0 = new MockGdi();
        wddm0->resetGdi(gdi0);
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm0);

        // Set up WDDM for root device 1
        wddm1 = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[1].get()));
        wddm1->init();
        gdi1 = new MockGdi();
        wddm1->resetGdi(gdi1);
        executionEnvironment.rootDeviceEnvironments[1]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm1);

        executionEnvironment.initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);

        // Create two command stream receivers for two GPUs
        csr1.reset(createCommandStream(executionEnvironment, 0u, 1));
        csr2.reset(createCommandStream(executionEnvironment, 1u, 1));

        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor(
            gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
            PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo));

        // Create two OS contexts simulating two GPU contexts
        osContext1 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr1.get(), engineDescriptor);
        osContext1->ensureContextInitialized(false);
        osContext1->incRefInternal();

        osContext2 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr2.get(), engineDescriptor);
        osContext2->ensureContextInitialized(false);
        osContext2->incRefInternal();

        residencyController1 = &static_cast<OsContextWin *>(osContext1)->getResidencyController();
        residencyController2 = &static_cast<OsContextWin *>(osContext2)->getResidencyController();

        csr1->setupContext(*osContext1);
        csr2->setupContext(*osContext2);

        gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    }

    void TearDown() override {
        osContext1->decRefInternal();
        osContext2->decRefInternal();
    }

    MockExecutionEnvironment executionEnvironment{nullptr, true, 2u};
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr1;
    std::unique_ptr<CommandStreamReceiver> csr2;

    WddmMock *wddm0 = nullptr;
    WddmMock *wddm1 = nullptr;
    OsContext *osContext1 = nullptr;
    OsContext *osContext2 = nullptr;
    MockGdi *gdi0 = nullptr;
    MockGdi *gdi1 = nullptr;
    WddmResidencyController *residencyController1 = nullptr;
    WddmResidencyController *residencyController2 = nullptr;
    GmmHelper *gmmHelper = nullptr;
};

TEST_F(WddmResidencyControllerMultiGpuSharedAllocationTest, givenSingleFragmentAllocationSharedAcrossGpusThroughMultiGraphicsAllocationThenResidencyTrackedIndependentlyPerContext) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    // Simulate two GPU devices sharing the same allocation
    RootDeviceIndicesContainer rootDeviceIndices = {0u, 1u};
    const auto size = MemoryConstants::pageSize;
    auto hostPtr = reinterpret_cast<void *>(wddm0->virtualAllocAddress + 0x1000);

    AllocationProperties properties(csr1->getRootDeviceIndex(), false, size, AllocationType::bufferHostMemory, false, {});
    MultiGraphicsAllocation multiAllocation(1u);

    // Pass host pointer to trigger fragment creation
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiAllocation, hostPtr);
    ASSERT_NE(nullptr, ptr);

    // Get allocation references for each GPU
    auto allocationForGpu1 = static_cast<WddmAllocation *>(multiAllocation.getGraphicsAllocation(0u));
    auto allocationForGpu2 = static_cast<WddmAllocation *>(multiAllocation.getGraphicsAllocation(1u));
    ASSERT_NE(nullptr, allocationForGpu1);
    ASSERT_NE(nullptr, allocationForGpu2);

    // Test expects fragment-based allocation and the allocation must have exactly one fragment
    ASSERT_EQ(1u, allocationForGpu1->fragmentsStorage.fragmentCount);
    ASSERT_EQ(1u, allocationForGpu2->fragmentsStorage.fragmentCount);

    // GPU1: First makeResident
    wddm0->callBaseMakeResident = true;
    wddm0->makeResidentResult.called = 0;
    ResidencyContainer residencyPackGpu1{allocationForGpu1};
    bool requiresBlockingResidencyHandling = true;
    residencyController1->makeResidentResidencyAllocations(residencyPackGpu1, requiresBlockingResidencyHandling, csr1.get());

    EXPECT_TRUE(allocationForGpu1->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId1]);
    EXPECT_EQ(1u, wddm0->makeResidentResult.called);

    // GPU2: First makeResident of same allocation
    wddm1->callBaseMakeResident = true;
    wddm1->makeResidentResult.called = 0;
    ResidencyContainer residencyPackGpu2{allocationForGpu2};
    residencyController2->makeResidentResidencyAllocations(residencyPackGpu2, requiresBlockingResidencyHandling, csr2.get());

    EXPECT_EQ(1u, wddm1->makeResidentResult.called);
    EXPECT_TRUE(allocationForGpu2->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId2]);

    // GPU1: Second makeResident - should not call WDDM (already resident)
    wddm0->makeResidentResult.called = 0;
    ResidencyContainer residencyPackGpu1Again{allocationForGpu1};
    residencyController1->makeResidentResidencyAllocations(residencyPackGpu1Again, requiresBlockingResidencyHandling, csr1.get());

    EXPECT_EQ(0u, wddm0->makeResidentResult.called);

    // GPU2: Second makeResident - should not call WDDM (already resident)
    wddm1->makeResidentResult.called = 0;
    ResidencyContainer residencyPackGpu2Again{allocationForGpu2};
    residencyController2->makeResidentResidencyAllocations(residencyPackGpu2Again, requiresBlockingResidencyHandling, csr2.get());

    EXPECT_EQ(0u, wddm1->makeResidentResult.called);

    // Simulate eviction on GPU1 only
    allocationForGpu1->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId1] = false;

    // GPU1: Re-make resident after eviction
    wddm0->makeResidentResult.called = 0;
    ResidencyContainer residencyPackGpu1AfterEvict{allocationForGpu1};
    residencyController1->makeResidentResidencyAllocations(residencyPackGpu1AfterEvict, requiresBlockingResidencyHandling, csr1.get());

    EXPECT_EQ(1u, wddm0->makeResidentResult.called);
    EXPECT_TRUE(allocationForGpu1->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId1]);

    // GPU2: Still resident, no WDDM call needed
    wddm1->makeResidentResult.called = 0;
    ResidencyContainer residencyPackGpu2AfterGpu1Evict{allocationForGpu2};
    residencyController2->makeResidentResidencyAllocations(residencyPackGpu2AfterGpu1Evict, requiresBlockingResidencyHandling, csr2.get());

    EXPECT_EQ(0u, wddm1->makeResidentResult.called);
    EXPECT_TRUE(allocationForGpu2->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId2]);

    for (auto &allocation : multiAllocation.getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(allocation);
    }
}

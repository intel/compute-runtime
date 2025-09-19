/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_context_win.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/mock_wddm_residency_controller.h"
#include "shared/test/common/mocks/mock_wddm_residency_logger.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

struct WddmResidencyControllerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->initializeMemoryManager();
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        wddm->init();
        mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, 0, osContextId, EngineDescriptorHelper::getDefaultDescriptor());
        wddm->getWddmInterface()->createMonitoredFence(*mockOsContextWin);
        residencyController = &mockOsContextWin->mockResidencyController;
        csr.reset(createCommandStream(*executionEnvironment, 0u, 1));
        residencyController->setCommandStreamReceiver(csr.get());
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    WddmMock *wddm = nullptr;
    std::unique_ptr<MockOsContextWin> mockOsContextWin;
    std::unique_ptr<CommandStreamReceiver> csr;
    NEO::MockWddmResidencyController *residencyController = nullptr;
};

struct WddmResidencyControllerWithGdiTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->initializeMemoryManager();
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        gdi = new MockGdi();
        wddm->resetGdi(gdi);
        wddm->init();

        mockOsContextWin = std::make_unique<MockOsContextWin>(*wddm, 0, osContextId, EngineDescriptorHelper::getDefaultDescriptor());
        wddm->getWddmInterface()->createMonitoredFence(*mockOsContextWin);
        csr.reset(createCommandStream(*executionEnvironment, 0u, 1));
        residencyController = &mockOsContextWin->mockResidencyController;
        residencyController->setCommandStreamReceiver(csr.get());
        residencyController->registerCallback();
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    WddmMock *wddm = nullptr;
    std::unique_ptr<MockOsContextWin> mockOsContextWin;
    std::unique_ptr<CommandStreamReceiver> csr;
    NEO::MockWddmResidencyController *residencyController = nullptr;
    MockGdi *gdi = nullptr;
};

struct WddmResidencyControllerWithMockWddmTest : public WddmResidencyControllerTest {
    void SetUp() override {
        wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
        wddm->resetGdi(new MockGdi());
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();

        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment.initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);

        csr.reset(createCommandStream(executionEnvironment, 0u, 1));
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                                      preemptionMode));
        csr->setupContext(*osContext);
        osContext->ensureContextInitialized(false);

        osContext->incRefInternal();
        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
        residencyController->setCommandStreamReceiver(csr.get());
        gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    }

    void TearDown() override {
        osContext->decRefInternal();
    }

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr;
    WddmMock *wddm = nullptr;
    OsContext *osContext;
    WddmResidencyController *residencyController;
    GmmHelper *gmmHelper = nullptr;
};

struct WddmResidencyControllerWithGdiAndMemoryManagerTest : ::testing::Test {
    const uint32_t osContextId = 0u;

    void SetUp() override {
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
        wddm->init();
        gdi = new MockGdi();
        wddm->resetGdi(gdi);

        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment.initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
        csr.reset(createCommandStream(executionEnvironment, 0u, 1));
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        osContext->ensureContextInitialized(false);

        osContext->incRefInternal();

        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
        residencyController->setCommandStreamReceiver(csr.get());
        gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    }

    void TearDown() override {
        osContext->decRefInternal();
    }

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr;

    WddmMock *wddm = nullptr;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
    WddmResidencyController *residencyController = nullptr;
    GmmHelper *gmmHelper = nullptr;
};

TEST(WddmResidencyController, givenWddmResidencyControllerWhenItIsConstructedThenDoNotRegisterTrimCallback) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto gdi = new MockGdi();
    auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
    wddm->resetGdi(gdi);
    wddm->init();

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));
    NEO::MockWddmResidencyController residencyController{*wddm, 0u};

    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    EXPECT_EQ(nullptr, residencyController.trimCallbackHandle);

    EXPECT_EQ(nullptr, gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(nullptr, gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(0u, gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST(WddmResidencyController, givenWddmResidencyControllerWhenRegisterCallbackThenCallbackIsSetUpProperly) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto gdi = new MockGdi();
    auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
    wddm->resetGdi(gdi);
    wddm->init();

    std::memset(&gdi->getRegisterTrimNotificationArg(), 0, sizeof(D3DKMT_REGISTERTRIMNOTIFICATION));

    WddmResidencyController residencyController{*wddm, 0u};
    residencyController.registerCallback();

    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
    EXPECT_EQ(reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmResidencyController::trimCallback), gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(reinterpret_cast<void *>(&residencyController), gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST_F(WddmResidencyControllerTest, givenWddmResidencyControllerWhenCallingWasAllocationUsedSinceLastTrimThenReturnCorrectValues) {
    residencyController->lastTrimFenceValue = 100;
    EXPECT_FALSE(residencyController->wasAllocationUsedSinceLastTrim(99));
    EXPECT_FALSE(residencyController->wasAllocationUsedSinceLastTrim(99));
    EXPECT_TRUE(residencyController->wasAllocationUsedSinceLastTrim(101));
}

TEST_F(WddmResidencyControllerTest, givenWddmResidencyControllerThenUpdateLastTrimFenceValueUsesMonitoredFence) {
    *residencyController->getMonitoredFence().cpuAddress = 1234;
    residencyController->updateLastTrimFenceValue();
    EXPECT_EQ(1234u, residencyController->lastTrimFenceValue);

    *residencyController->getMonitoredFence().cpuAddress = 12345;
    residencyController->updateLastTrimFenceValue();
    EXPECT_EQ(12345u, residencyController->lastTrimFenceValue);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenWddmResidencyControllerWhenItIsDestructedThenUnregisterTrimCallback) {
    auto trimCallbackHandle = residencyController->trimCallbackHandle;
    auto trimCallbackAddress = reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(WddmResidencyController::trimCallback);

    std::memset(&gdi->getUnregisterTrimNotificationArg(), 0, sizeof(D3DKMT_UNREGISTERTRIMNOTIFICATION));
    mockOsContextWin.reset();

    EXPECT_EQ(trimCallbackAddress, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(trimCallbackHandle, gdi->getUnregisterTrimNotificationArg().Handle);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenWddmResidencyControllerWhenItIsDestructedDuringProcessShutdownThenDontUnregisterTrimCallback) {
    wddm->shutdownStatus = true;

    std::memset(&gdi->getUnregisterTrimNotificationArg(), 0, sizeof(D3DKMT_UNREGISTERTRIMNOTIFICATION));
    mockOsContextWin.reset();

    EXPECT_EQ(nullptr, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(nullptr, gdi->getUnregisterTrimNotificationArg().Handle);
}

TEST_F(WddmResidencyControllerTest, givenWddmResidencyControllerWhenIsMemoryExhaustedIsCalledThenReturnCorrectResult) {
    EXPECT_FALSE(residencyController->isMemoryBudgetExhausted());
    residencyController->setMemoryBudgetExhausted();
    EXPECT_TRUE(residencyController->isMemoryBudgetExhausted());
}

TEST_F(WddmResidencyControllerWithGdiTest, givenNotUsedAllocationsFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenAllocationsAreEvictedMarkedAndRemovedFromEvictionContainer) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(1);

    auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);

    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().updateCompletionData(0, osContextId);
    allocation2.getResidencyData().updateCompletionData(0, osContextId);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation2.getResidencyData().resident[osContextId] = true;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->evictResult.called = 0;
    wddm->callBaseEvict = true;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // Single evict called
    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_EQ(1u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    EXPECT_TRUE(csr->getEvictionAllocations().empty());
    // marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenOneUsedAllocationFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenOneAllocationIsTrimmed) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmHelper());
    allocation1.getResidencyData().resident[osContextId] = true;
    // mark allocation used from last periodic trim
    allocation1.getResidencyData().updateCompletionData(0, osContextId);
    allocation2.getResidencyData().updateCompletionData(11, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 1 allocation evicted
    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_EQ(1u, csr->getEvictionAllocations().size());

    // marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    // second stays resident
    EXPECT_TRUE(allocation2.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, givenTripleAllocationWithUsedAndUnusedFragmentsSincePreviousTrimWhenTrimResidencyPeriodicTrimIsCalledThenProperFragmentsAreEvictedAndMarked) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer{};
    debugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(1);

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);

    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // 3-fragment Allocation
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    auto allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptr));

    // whole allocation unused since previous trim
    allocationTriple->getResidencyData().updateCompletionData(0, osContextId);

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->updateCompletionData(0, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId] = true;
    // this fragment was used
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(11, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId] = true;

    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->updateCompletionData(0, osContextId);
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident[osContextId] = true;

    // Set last periodic fence value
    *residencyController->getMonitoredFence().cpuAddress = 10;
    residencyController->updateLastTrimFenceValue();
    // Set current fence value to greater value
    residencyController->getMonitoredFence().currentFenceValue = 20;

    wddm->evictResult.called = 0;
    wddm->callBaseEvict = true;

    csr->getEvictionAllocations().push_back(allocationTriple);

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 fragments evicted with one call
    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_EQ(1u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    // marked nonresident
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId]);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident[osContextId]);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    *residencyController->getMonitoredFence().cpuAddress = 20;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, residencyController->lastTrimFenceValue);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenRestartPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.RestartPeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    residencyController->lastTrimFenceValue = 10;
    // Set current fence value to greater value
    *residencyController->getMonitoredFence().cpuAddress = 20;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, residencyController->lastTrimFenceValue);
}

HWTEST_F(WddmResidencyControllerWithGdiTest, GivenZeroWhenTrimmingToBudgetThenTrueIsReturnedAndDrainPagingFenceQueueCalled) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(csr.get());
    EXPECT_EQ(0u, ultCsr->drainPagingFenceQueueCalled);
    bool status = residencyController->trimResidencyToBudget(0);
    EXPECT_TRUE(status);
    EXPECT_EQ(1u, ultCsr->drainPagingFenceQueueCalled);
}

TEST_F(WddmResidencyControllerWithGdiTest, WhenTrimmingToBudgetThenAllDoneAllocationsAreTrimmed) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;
    wddm->callBaseEvict = true;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);
    csr->getEvictionAllocations().push_back(&allocation3);

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_EQ(0u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    EXPECT_EQ(1u, csr->getEvictionAllocations().size());

    auto iter = std::find(csr->getEvictionAllocations().begin(), csr->getEvictionAllocations().end(), &allocation3);
    EXPECT_NE(iter, csr->getEvictionAllocations().end());
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenNumBytesToTrimIsNotZeroWhenTrimmingToBudgetThenFalseIsReturned) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation1);

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_TRUE(csr->getEvictionAllocations().empty());

    EXPECT_FALSE(status);
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenNumBytesToTrimIsZeroWhenTrimmingToBudgetThenEvictingStops) {
    auto ptr = reinterpret_cast<void *>(0x1000);
    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation1(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 0x1000, nullptr, MemoryPool::memoryNull, 0u, 1u);
    WddmAllocation allocation2(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 0x3000, nullptr, MemoryPool::memoryNull, 0u, 1u);
    WddmAllocation allocation3(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 0x1000, nullptr, MemoryPool::memoryNull, 0u, 1u);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);
    csr->getEvictionAllocations().push_back(&allocation3);

    bool status = residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_TRUE(status);
    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_EQ(1u, csr->getEvictionAllocations().size());
    auto iter = std::find(csr->getEvictionAllocations().begin(), csr->getEvictionAllocations().end(), &allocation3);
    EXPECT_NE(iter, csr->getEvictionAllocations().end());
}

TEST_F(WddmResidencyControllerWithGdiTest, WhenTrimmingToBudgetThenEvictedAllocationIsMarkedNonResident) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);
    csr->getEvictionAllocations().push_back(&allocation3);

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenAlwaysResidentAllocationWhenTrimResidencyToBudgetCalledThenDontEvict) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());

    allocation.getResidencyData().resident[osContextId] = true;
    allocation.getResidencyData().updateCompletionData(0, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation);
    allocation.updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, osContextId);

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_TRUE(allocation.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenAlwaysResidentAllocationWhenTrimResidencyCalledThenDontEvict) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());

    allocation.getResidencyData().resident[osContextId] = true;
    allocation.getResidencyData().updateCompletionData(0, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation);
    allocation.updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, osContextId);

    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;
    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_TRUE(allocation.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenLastFenceIsGreaterThanMonitoredWhenTrimmingToBudgetThenWaitForCpu) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(2, osContextId);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 2;
    residencyController->getMonitoredFence().currentFenceValue = 3;

    wddm->evictResult.called = 0;
    wddm->waitFromCpuResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation1);

    gdi->getWaitFromCpuArg().hDevice = 0;
    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);

    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getWaitFromCpuArg().hDevice);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, WhenTrimmingToBudgetThenOnlyDoneFragmentsAreEvicted) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    gdi->setNonZeroNumBytesToTrimInEvict();
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation1(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 0x1000, nullptr, MemoryPool::memoryNull, 0u, 1u);
    WddmAllocation allocation2(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 0x1000, nullptr, MemoryPool::memoryNull, 0u, 1u);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    void *ptrTriple = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 0x500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptrTriple));

    allocationTriple->getResidencyData().updateCompletionData(1, osContextId);
    allocationTriple->getResidencyData().resident[osContextId] = true;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    for (uint32_t i = 0; i < 3; i++) {
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->updateCompletionData(1, osContextId);
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId] = true;
    }

    // This should not be evicted
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(2, osContextId);

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(allocationTriple);
    csr->getEvictionAllocations().push_back(&allocation2);

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 2;

    wddm->evictResult.called = 0;
    wddm->callBaseEvict = true;

    residencyController->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->evictResult.called);
    EXPECT_EQ(0u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident[osContextId]);
    EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident[osContextId]);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident[osContextId]);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiTest, givenThreeAllocationsAlignedSizeBiggerThanAllocSizeWhenBudgetEqualTwoAlignedAllocationThenEvictOnlyTwo) {
    gdi->setNonZeroNumBytesToTrimInEvict();
    size_t underlyingSize = 0xF00;
    size_t alignedSize = 0x1000;
    size_t budget = 2 * alignedSize;

    // trim budget should consider aligned size, not underlying, so if function considers underlying, it should evict three, not two
    EXPECT_GT((3 * underlyingSize), budget);
    EXPECT_LT((2 * underlyingSize), budget);
    void *ptr1 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    void *ptr2 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x3000);
    void *ptr3 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x5000);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    WddmAllocation allocation1(0, 1u /*num gmms*/, AllocationType::unknown, ptr1, gmmHelper->canonize(castToUint64(const_cast<void *>(ptr1))), underlyingSize, nullptr, MemoryPool::memoryNull, 0u, 1u);
    WddmAllocation allocation2(0, 1u /*num gmms*/, AllocationType::unknown, ptr2, gmmHelper->canonize(castToUint64(const_cast<void *>(ptr2))), underlyingSize, nullptr, MemoryPool::memoryNull, 0u, 1u);
    WddmAllocation allocation3(0, 1u /*num gmms*/, AllocationType::unknown, ptr3, gmmHelper->canonize(castToUint64(const_cast<void *>(ptr3))), underlyingSize, nullptr, MemoryPool::memoryNull, 0u, 1u);

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(1, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);
    csr->getEvictionAllocations().push_back(&allocation3);

    bool status = residencyController->trimResidencyToBudget(budget);
    EXPECT_TRUE(status);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
}

using WddmResidencyControllerLockTest = WddmResidencyControllerWithGdiTest;

TEST_F(WddmResidencyControllerLockTest, givenPeriodicTrimWhenTrimmingResidencyThenLockOnce) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(1u, residencyController->acquireLockCallCount);
}

TEST_F(WddmResidencyControllerLockTest, givenTrimToBudgetWhenTrimmingResidencyThenLockOnce) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.TrimToBudget = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(1u, residencyController->acquireLockCallCount);
}

HWTEST_F(WddmResidencyControllerLockTest, givenTrimToBudgetWhenTrimmingToBudgetThenLockCsr) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.TrimToBudget = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(1u, static_cast<UltCommandStreamReceiver<FamilyType> *>(residencyController->csr)->recursiveLockCounter);
}

TEST_F(WddmResidencyControllerLockTest, givenPeriodicTrimAndTrimToBudgetWhenTrimmingResidencyThenLockTwice) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.Flags.TrimToBudget = 1;
    trimNotification.NumBytesToTrim = 0;

    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    EXPECT_EQ(2u, residencyController->acquireLockCallCount);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, WhenMakingResidentResidencyAllocationsThenAllAllocationsAreMarked) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    MockWddmAllocation allocation3(gmmHelper);
    MockWddmAllocation allocation4(gmmHelper);
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool requiresBlockingResidencyHandling = true;
    residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation3.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocation4.getResidencyData().resident[osContextId]);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, WhenMakingResidentResidencyAllocationsThenLastFenceIsUpdated) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    MockWddmAllocation allocation3(gmmHelper);
    MockWddmAllocation allocation4(gmmHelper);
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    residencyController->getMonitoredFence().currentFenceValue = 20;
    bool requiresBlockingResidencyHandling = true;
    residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_EQ(20u, allocation1.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation2.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation3.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation4.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, GivenTripleAllocationsWhenMakingResidentResidencyAllocationsThenAllAllocationsAreMarkedResident) {
    if (executionEnvironment.memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    wddm->callBaseMakeResident = true;

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptr);
    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    bool requiresBlockingResidencyHandling = true;
    residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId]);
    }

    EXPECT_EQ(EngineLimits::maxHandleCount + 3 + EngineLimits::maxHandleCount, gdi->getMakeResidentArg().NumAllocations);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithGdiAndMemoryManagerTest, GivenTripleAllocationsWhenMakingResidentResidencyAllocationsThenLastFencePlusOneIsSet) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);

    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, reinterpret_cast<void *>(0x1500)));

    residencyController->getMonitoredFence().currentFenceValue = 20;
    bool requiresBlockingResidencyHandling = true;
    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(20u, allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->getFenceValueForContextId(0));
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenDontMarkAllocationsAsResident) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    MockWddmAllocation allocation3(gmmHelper);
    MockWddmAllocation allocation4(gmmHelper);

    wddm->makeResidentNumberOfBytesToTrim = 4 * 4096;
    wddm->makeResidentStatus = false;

    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation2.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation3.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocation4.getResidencyData().resident[osContextId]);
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenResidencyContainerIsCleared) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    MockWddmAllocation allocation3(gmmHelper);
    MockWddmAllocation allocation4(gmmHelper);

    wddm->makeResidentNumberOfBytesToTrim = 4 * 4096;
    wddm->makeResidentStatus = false;

    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_FALSE(result);
    EXPECT_EQ(residencyPack.size(), 0u);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenDontMarkTripleAllocationsAsResident) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    wddm->callBaseCreateAllocationsAndMapGpuVa = true;
    void *ptr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize}, ptr));
    ASSERT_NE(nullptr, allocationTriple);

    wddm->makeResidentNumberOfBytesToTrim = 4 * 4096;
    wddm->makeResidentStatus = false;

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_FALSE(result);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident[osContextId]);
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenCallItAgainWithCantTrimFurtherSetToTrue) {
    MockWddmAllocation allocation1(gmmHelper);
    allocation1.getResidencyData().updateCompletionData(0, osContextId);
    residencyController->getMonitoredFence().currentFenceValue = 10;

    wddm->makeResidentNumberOfBytesToTrim = 4 * 4096;
    wddm->makeResidentStatus = false;

    ResidencyContainer residencyPack{&allocation1};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_FALSE(result);
    EXPECT_NE(wddm->makeResidentParamsPassed[0].cantTrimFurther, wddm->makeResidentParamsPassed[1].cantTrimFurther);
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
    EXPECT_EQ(0u, allocation1.getResidencyData().getFenceValueForContextId(osContextId));
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenAllocationPackPassedWhenCallingMakeResidentResidencyAllocationsThenItIsUsed) {
    MockWddmAllocation allocation1(gmmHelper);
    MockWddmAllocation allocation2(gmmHelper);
    allocation1.handle = 1;
    allocation2.handle = 2;
    ResidencyContainer residencyPack{&allocation1, &allocation2};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);
    EXPECT_TRUE(result);
    EXPECT_EQ(2 * EngineLimits::maxHandleCount, wddm->makeResidentResult.handleCount);
    EXPECT_EQ(false, wddm->makeResidentResult.cantTrimFurther);
    EXPECT_EQ(1u, wddm->makeResidentResult.handlePack[0 * EngineLimits::maxHandleCount]);
    EXPECT_EQ(2u, wddm->makeResidentResult.handlePack[1 * EngineLimits::maxHandleCount]);
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsAndTrimToBudgetSucceedsWhenCallingMakeResidentResidencyAllocationsThenSucceed) {
    MockWddmAllocation allocation1(gmmHelper);
    void *cpuPtr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1000);
    size_t allocationSize = 0x1000;
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(cpuPtr)));
    WddmAllocation allocationToTrim(0, 1u /*num gmms*/, AllocationType::unknown, cpuPtr, canonizedAddress, allocationSize, nullptr, MemoryPool::memoryNull, 0u, 1u);

    allocationToTrim.getResidencyData().updateCompletionData(residencyController->getMonitoredFence().lastSubmittedFence, osContextId);
    allocationToTrim.getResidencyData().resident[osContextId] = true;

    csr->getEvictionAllocations().push_back(&allocationToTrim);
    wddm->makeResidentNumberOfBytesToTrim = allocationSize;
    wddm->makeResidentResults = {false, true};

    ResidencyContainer residencyPack{&allocation1};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_FALSE(allocationToTrim.getResidencyData().resident[osContextId]);
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
    EXPECT_EQ(0u, csr->getEvictionAllocations().size());
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsAndTrimToBudgetSucceedsWhenCallingMakeResidentWithTrimmedAllocationThenSucceed) {
    MockWddmAllocation allocation1(gmmHelper);
    void *cpuPtr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1000);
    size_t allocationSize = 0x1000;
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(cpuPtr)));
    WddmAllocation allocationAlreadyResident(0, 1u /*num gmms*/, AllocationType::unknown, cpuPtr, canonizedAddress, allocationSize, nullptr, MemoryPool::memoryNull, 0u, 1u);

    allocationAlreadyResident.getResidencyData().updateCompletionData(residencyController->getMonitoredFence().lastSubmittedFence, osContextId);
    allocationAlreadyResident.getResidencyData().resident[osContextId] = true;

    csr->getEvictionAllocations().push_back(&allocationAlreadyResident);
    wddm->makeResidentNumberOfBytesToTrim = allocationSize;
    wddm->makeResidentResults = {false, true};

    ResidencyContainer residencyPack{&allocation1, &allocationAlreadyResident};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident[osContextId]);
    EXPECT_TRUE(allocationAlreadyResident.getResidencyData().resident[osContextId]);
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
    EXPECT_EQ(0u, csr->getEvictionAllocations().size());
    EXPECT_EQ(2u, residencyPack.size());
}

TEST_F(WddmResidencyControllerWithMockWddmTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenMemoryBudgetExhaustedIsSetToTrue) {
    MockWddmAllocation allocation1(gmmHelper);
    ResidencyContainer residencyPack{&allocation1};

    wddm->makeResidentResults = {false, true};
    bool requiresBlockingResidencyHandling = true;
    residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);
    EXPECT_TRUE(residencyController->isMemoryBudgetExhausted());
    EXPECT_EQ(2u, wddm->makeResidentResult.called);
}

struct WddmMakeResidentMock : public WddmMock {
    WddmMakeResidentMock(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment){};

    bool makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) override {
        *numberOfBytesToTrim = makeResidentNumberOfBytesToTrim;
        makeResidentResult.called++;

        if (makeResidentResult.called > 2) {
            return true;
        } else {
            return false;
        }
    }
};

struct WddmResidencyControllerWithMockWddmMakeResidentTest : public WddmResidencyControllerWithMockWddmTest {
    void SetUp() override {
        wddm = new WddmMakeResidentMock(*executionEnvironment.rootDeviceEnvironments[0].get());
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();

        executionEnvironment.initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);

        csr.reset(createCommandStream(executionEnvironment, 0u, 1));
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                                      preemptionMode));

        osContext->incRefInternal();
        residencyController = &static_cast<OsContextWin *>(osContext)->getResidencyController();
        residencyController->setCommandStreamReceiver(csr.get());
        gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    }
};

TEST_F(WddmResidencyControllerWithMockWddmMakeResidentTest, givenMakeResidentFailsWhenCallingMakeResidentResidencyAllocationsThenCallItAgainWithWaitForMemoryReleaseSetToTrue) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.WaitForMemoryRelease.set(1);

    wddm->makeResidentNumberOfBytesToTrim = 4 * 4096;

    MockWddmAllocation allocation1(gmmHelper);
    ResidencyContainer residencyPack{&allocation1};
    bool requiresBlockingResidencyHandling = true;
    bool result = residencyController->makeResidentResidencyAllocations(residencyPack, requiresBlockingResidencyHandling);

    EXPECT_TRUE(result);
    EXPECT_EQ(3u, wddm->makeResidentResult.called);
}

TEST_F(WddmResidencyControllerTest, GivenResidencyLoggingEnabledWhenTrimResidencyCalledThenExpectLogData) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());
    ASSERT_NE(nullptr, logger);

    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    residencyController->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    int64_t timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(logger->callbackStartSave - now).count();
    EXPECT_LE(0, timeDiff);
    EXPECT_EQ(trimNotification.Flags.Value, logger->callbackFlagSave);
    EXPECT_EQ(residencyController, logger->controllerObjectSave);

    // 2 - one for open log, second for trim callback
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
}

TEST_F(WddmResidencyControllerWithGdiTest, GivenResidencyLoggingEnabledWhenTrimmingToBudgetThenExpectLogData) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());
    ASSERT_NE(nullptr, logger);

    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation2(rootDeviceEnvironment->getGmmHelper());
    MockWddmAllocation allocation3(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().resident[osContextId] = true;
    allocation1.getResidencyData().updateCompletionData(0, osContextId);

    allocation2.getResidencyData().updateCompletionData(1, osContextId);
    allocation2.getResidencyData().resident[osContextId] = true;

    allocation3.getResidencyData().updateCompletionData(2, osContextId);
    allocation3.getResidencyData().resident[osContextId] = true;

    *residencyController->getMonitoredFence().cpuAddress = 1;
    residencyController->getMonitoredFence().lastSubmittedFence = 1;
    residencyController->getMonitoredFence().currentFenceValue = 1;

    wddm->evictResult.called = 0;
    wddm->callBaseEvict = true;

    csr->getEvictionAllocations().push_back(&allocation1);
    csr->getEvictionAllocations().push_back(&allocation2);
    csr->getEvictionAllocations().push_back(&allocation3);

    constexpr uint64_t trimBudgetSize = 3 * 4096;
    residencyController->trimResidencyToBudget(trimBudgetSize);

    EXPECT_EQ(trimBudgetSize, logger->numBytesToTrimSave);
    EXPECT_EQ(residencyController, logger->controllerObjectSave);

    // 2 - one for open log, second for trim to budget
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
}

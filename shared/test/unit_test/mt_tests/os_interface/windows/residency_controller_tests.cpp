/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context_win.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <atomic>
#include <functional>

using namespace NEO;

struct WddmResidencyControllerMTTest : ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->initializeMemoryManager();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        wddm->init();
        auto ccsDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
        auto bcsDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular});
        csrCCS = std::make_unique<MockCommandStreamReceiver>(*executionEnvironment, 0u, 1);
        csrBCS = std::make_unique<MockCommandStreamReceiver>(*executionEnvironment, 0u, 1);
        auto osContextCCS = executionEnvironment->memoryManager->createAndRegisterOsContext(csrCCS.get(), ccsDescriptor);
        auto osContextBCS = executionEnvironment->memoryManager->createAndRegisterOsContext(csrBCS.get(), bcsDescriptor);
        csrCCS->setupContext(*osContextCCS);
        csrBCS->setupContext(*osContextBCS);
        osContextCCS->ensureContextInitialized();
        osContextBCS->ensureContextInitialized();
        residencyController = &wddm->getResidencyController();
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment;
    WddmMock *wddm = nullptr;
    std::unique_ptr<MockCommandStreamReceiver> csrCCS;
    std::unique_ptr<MockCommandStreamReceiver> csrBCS;
    NEO::WddmResidencyController *residencyController = nullptr;
};

TEST_F(WddmResidencyControllerMTTest, givenAllocationsToTrimWhenCCSCsrIsLockedThenTrimWaitsOnCCSCsrLockWithLockedBCSCsr) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 0;
    trimNotification.NumBytesToTrim = std::numeric_limits<uint32_t>::max();
    trimNotification.Flags.TrimToBudget = true;

    MockWddmAllocation allocation1(rootDeviceEnvironment->getGmmHelper());

    allocation1.getResidencyData().updateCompletionData(25, csrCCS->getOsContext().getContextId());
    allocation1.getResidencyData().resident[csrCCS->getOsContext().getContextId()] = true;
    wddm->getResidencyController().getEvictionAllocations().push_back(&allocation1);

    allocation1.getResidencyData().updateCompletionData(0, csrBCS->getOsContext().getContextId());
    allocation1.getResidencyData().resident[csrBCS->getOsContext().getContextId()] = true;
    wddm->getResidencyController().getEvictionAllocations().push_back(&allocation1);

    std::atomic<bool> threadOwnOwnership = false;
    std::thread th = std::thread([&]() {
        auto ccsCsrLock = csrCCS->obtainUniqueOwnership();
        threadOwnOwnership = true;
        while (!csrBCS->isOwnershipMutexLocked()) {
        }
        EXPECT_TRUE(csrBCS->isOwnershipMutexLocked());
        ccsCsrLock.unlock();
    });
    std::thread th2 = std::thread([&]() {
        while (!threadOwnOwnership) {
        }
        wddm->getResidencyController().trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    });
    th.join();
    th2.join();
    EXPECT_EQ(csrCCS->obtainUniqueOwnershipCalledTimes, 2u);
}

struct WddmResidencyControllerContextGroupMTTest : ::testing::Test {
    void SetUp() override {
        executionEnvironment.initializeMemoryManager();
        auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        wddm->resetGdi(new MockGdi());
        wddm->init();
        trimNotification.Flags.TrimToBudget = true;
        trimNotification.NumBytesToTrim = std::numeric_limits<uint32_t>::max();
    }

    void registerContextGroup(CommandStreamReceiver &primaryCsr, CommandStreamReceiver &secondaryCsr) {
        auto ccsDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
        auto &engines = const_cast<EngineControlContainer &>(executionEnvironment.memoryManager->getRegisteredEngines(0));
        for (auto csr : {&primaryCsr, &secondaryCsr}) {
            osContexts.push_back(std::make_unique<MockOsContextWin>(*wddm, 0u, static_cast<uint32_t>(osContexts.size()), ccsDescriptor));
            osContexts.back()->contextInitialized = true;
            wddm->getWddmInterface()->createMonitoredFence(*osContexts.back());
            csr->setupContext(*osContexts.back());
            engines.emplace_back(csr, osContexts.back().get());
        }
        secondaryCsr.setPrimaryCsr(&primaryCsr);
    }

    void unregisterEngines() {
        const_cast<EngineControlContainer &>(executionEnvironment.memoryManager->getRegisteredEngines(0)).clear();
    }

    void trim() {
        wddm->getResidencyController().trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);
    }

    MockExecutionEnvironment executionEnvironment;
    WddmMock *wddm = nullptr;
    std::vector<std::unique_ptr<MockOsContextWin>> osContexts;
    D3DKMT_TRIMNOTIFICATION trimNotification = {};
};

HWTEST2_F(WddmResidencyControllerContextGroupMTTest, givenSecondaryCsrHeldBySubmissionWhenItSetsScratchAddressDuringTrimThenBothComplete, IsAtLeastXe3pCore) {
    UltCommandStreamReceiver<FamilyType> primaryCsr(executionEnvironment, 0u, 1);
    UltCommandStreamReceiver<FamilyType> secondaryCsr(executionEnvironment, 0u, 1);
    registerContextGroup(primaryCsr, secondaryCsr);

    constexpr size_t sshSize = 512;
    auto sshBuffer = alignedMalloc(sshSize, MemoryConstants::pageSize);
    MockGraphicsAllocation sshAllocation(sshBuffer, sshSize);
    IndirectHeap ssh(&sshAllocation);

    std::atomic<bool> secondaryCsrLocked = false;
    std::atomic<bool> trimDone = false;
    uint64_t scratchAddress = 0;
    std::thread submission([&]() {
        auto secondaryCsrLock = secondaryCsr.obtainUniqueOwnership();
        secondaryCsrLocked = true;
        while (primaryCsr.tryObtainUniqueOwnership().owns_lock() && !trimDone) {
        }
        EncodeDispatchKernel<FamilyType>::setScratchAddress(scratchAddress, 64u, 0u, &ssh, secondaryCsr);
    });
    std::thread trimThread([&]() {
        while (!secondaryCsrLocked) {
        }
        trim();
        trimDone = true;
    });
    submission.join();
    trimThread.join();

    EXPECT_NE(0u, scratchAddress);
    unregisterEngines();
    alignedFree(sshBuffer);
}

TEST_F(WddmResidencyControllerContextGroupMTTest, givenPrimaryCsrHeldByThreadThatLocksSecondaryCsrNextWhenTrimmingToBudgetThenBothComplete) {
    MockCommandStreamReceiver primaryCsr(executionEnvironment, 0u, 1);
    MockCommandStreamReceiver secondaryCsr(executionEnvironment, 0u, 1);
    registerContextGroup(primaryCsr, secondaryCsr);

    primaryCsr.obtainUniqueOwnershipCalledTimes = 0;
    std::atomic<bool> primaryCsrLocked = false;
    std::thread dependency([&]() {
        auto primaryCsrLock = primaryCsr.obtainUniqueOwnership();
        primaryCsrLocked = true;
        while (primaryCsr.obtainUniqueOwnershipCalledTimes < 2u) {
        }
        auto secondaryCsrLock = secondaryCsr.obtainUniqueOwnership();
    });
    std::thread trimThread([&]() {
        while (!primaryCsrLocked) {
        }
        trim();
    });
    dependency.join();
    trimThread.join();
    unregisterEngines();
}

struct WddmMockFailingFirstMakeResident : WddmMock {
    using WddmMock::WddmMock;
    bool makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) override {
        if (failNextMakeResident) {
            failNextMakeResident = false;
            onFailingMakeResident();
            *numberOfBytesToTrim = MemoryConstants::pageSize;
            return false;
        }
        return true;
    }
    std::function<void()> onFailingMakeResident;
    bool failNextMakeResident = true;
};

TEST(WddmResidencyControllerMakeResidentMTTest, givenSubmissionTrimmingAnotherSecondaryCsrInMakeResidentWhenTrimCallbackRunsThenBothComplete) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    auto wddm = new WddmMockFailingFirstMakeResident(*rootDeviceEnvironment);
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->resetGdi(new MockGdi());
    wddm->init();
    wddm->callBaseEvict = false;

    MockCommandStreamReceiver primaryCsr(executionEnvironment, 0u, 1);
    MockCommandStreamReceiver submittingCsr(executionEnvironment, 0u, 1);
    MockCommandStreamReceiver otherSecondaryCsr(executionEnvironment, 0u, 1);
    submittingCsr.setPrimaryCsr(&primaryCsr);
    otherSecondaryCsr.setPrimaryCsr(&primaryCsr);
    uint64_t completedFences[3] = {};
    std::vector<std::unique_ptr<MockOsContextWin>> osContexts;
    auto ccsDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
    auto &engines = const_cast<EngineControlContainer &>(executionEnvironment.memoryManager->getRegisteredEngines(0));
    for (auto csr : {&primaryCsr, &submittingCsr, &otherSecondaryCsr}) {
        auto contextId = static_cast<uint32_t>(osContexts.size());
        osContexts.push_back(std::make_unique<MockOsContextWin>(*wddm, 0u, contextId, ccsDescriptor));
        osContexts.back()->contextInitialized = true;
        wddm->getWddmInterface()->createMonitoredFence(*osContexts.back());
        osContexts.back()->getMonitoredFence().cpuAddress = &completedFences[contextId];
        osContexts.back()->getMonitoredFence().lastSubmittedFence = 1;
        osContexts.back()->getMonitoredFence().currentFenceValue = 2;
        csr->setupContext(*osContexts.back());
        engines.emplace_back(csr, osContexts.back().get());
    }

    MockWddmAllocation allocationOnOtherSecondary(rootDeviceEnvironment->getGmmHelper(), 1);
    allocationOnOtherSecondary.getResidencyData().resident[2] = true;
    allocationOnOtherSecondary.getResidencyData().updateCompletionData(1, 2);
    auto &residencyController = wddm->getResidencyController();
    residencyController.getEvictionAllocations().push_back(&allocationOnOtherSecondary);
    MockWddmAllocation newAllocation(rootDeviceEnvironment->getGmmHelper(), 1);
    ResidencyContainer allocationsForResidency{&newAllocation};

    std::atomic<bool> insideMakeResident = false;
    wddm->onFailingMakeResident = [&]() {
        insideMakeResident = true;
        while (primaryCsr.tryObtainUniqueOwnership().owns_lock()) {
        }
    };
    std::thread submission([&]() {
        auto csrLock = submittingCsr.obtainUniqueOwnership();
        bool requiresBlockingResidencyHandling = false;
        EXPECT_TRUE(residencyController.makeResidentResidencyAllocations(allocationsForResidency, requiresBlockingResidencyHandling, &submittingCsr));
    });
    std::thread trimCallback([&]() {
        while (!insideMakeResident) {
        }
        D3DDDI_TRIMRESIDENCYSET_FLAGS flags = {};
        flags.TrimToBudget = 1;
        residencyController.trimResidency(flags, MemoryConstants::pageSize);
    });
    submission.join();
    trimCallback.join();

    EXPECT_FALSE(allocationOnOtherSecondary.getResidencyData().resident[2]);
    residencyController.getEvictionAllocations().clear();
    engines.clear();
}

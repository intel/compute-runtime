/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_os_context_win.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"

#include "gtest/gtest.h"

#include <atomic>

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
        osContextCCS->ensureContextInitialized(false);
        osContextBCS->ensureContextInitialized(false);
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
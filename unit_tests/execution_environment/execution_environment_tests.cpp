/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/aub/aub_center.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "test.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/utilities/destructor_counted.h"

using namespace NEO;

TEST(ExecutionEnvironment, givenDefaultConstructorWhenItIsCalledThenExecutionEnvironmentHasInitialRefCountZero) {
    ExecutionEnvironment environment;
    EXPECT_EQ(0, environment.getRefInternalCount());
    EXPECT_EQ(0, environment.getRefApiCount());
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsConstructedThenItCretesExecutionEnvironmentWithOneRefCountInternal) {
    std::unique_ptr<Platform> platform(new Platform);
    ASSERT_NE(nullptr, platform->peekExecutionEnvironment());
    EXPECT_EQ(1, platform->peekExecutionEnvironment()->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenPlatformAndExecutionEnvironmentWithRefCountsWhenPlatformIsDestroyedThenExecutionEnvironmentIsNotDeleted) {
    std::unique_ptr<Platform> platform(new Platform);
    auto executionEnvironment = platform->peekExecutionEnvironment();
    executionEnvironment->incRefInternal();
    platform.reset();
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());
    executionEnvironment->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsInitializedAndCreatesDevicesThenThoseDevicesAddRefcountsToExecutionEnvironment) {
    std::unique_ptr<Platform> platform(new Platform);
    auto executionEnvironment = platform->peekExecutionEnvironment();

    platform->initialize();
    EXPECT_LT(0u, platform->getNumDevices());
    EXPECT_EQ(static_cast<int32_t>(1u + platform->getNumDevices()), executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenDeviceThatHaveRefferencesAfterPlatformIsDestroyedThenDeviceIsStillUsable) {
    std::unique_ptr<Platform> platform(new Platform);
    auto executionEnvironment = platform->peekExecutionEnvironment();
    platform->initialize();
    auto device = platform->getDevice(0);
    EXPECT_EQ(1, device->getRefInternalCount());
    device->incRefInternal();
    platform.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());

    device->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesCommandStreamReceiverInExecutionEnvironment) {
    Platform platform;
    auto executionEnvironment = platform.peekExecutionEnvironment();
    platform.initialize();
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0][0].get());
}

TEST(ExecutionEnvironment, whenPlatformIsInitializedThenOnlySpecialCommandStreamReceiverIsMultiOsContextCapable) {
    Platform platform;
    auto executionEnvironment = platform.peekExecutionEnvironment();
    platform.initialize();
    for (auto &csrContainer : executionEnvironment->commandStreamReceivers) {
        for (auto &csr : csrContainer) {
            EXPECT_FALSE(csr->isMultiOsContextCapable());
        }
    }
    if (executionEnvironment->specialCommandStreamReceiver) {
        EXPECT_TRUE(executionEnvironment->specialCommandStreamReceiver->isMultiOsContextCapable());
    }
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesMemoryManagerInExecutionEnvironment) {
    Platform platform;
    auto executionEnvironment = platform.peekExecutionEnvironment();
    platform.initialize();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenDeviceWhenItIsDestroyedThenMemoryManagerIsStillAvailable) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<Device> device(Device::create<Device>(executionEnvironment, 0u));
    device.reset(nullptr);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeCommandStreamReceiverIsCalledThenItIsInitalized) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeCommandStreamReceiver(0, 0);
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0][0]);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeIsCalledWithDifferentDeviceIndexesThenInternalStorageIsResized) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    EXPECT_EQ(0u, executionEnvironment->commandStreamReceivers.size());
    executionEnvironment->initializeCommandStreamReceiver(0, 0);
    EXPECT_EQ(1u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0][0]);
    executionEnvironment->initializeCommandStreamReceiver(1, 0);
    EXPECT_EQ(2u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[1][0]);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeIsCalledMultipleTimesForTheSameIndexThenCommandStreamReceiverIsReused) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    EXPECT_EQ(0u, executionEnvironment->commandStreamReceivers.size());
    executionEnvironment->initializeCommandStreamReceiver(0, 1);

    auto currentCommandStreamReceiver = executionEnvironment->commandStreamReceivers[0][1].get();
    executionEnvironment->initializeCommandStreamReceiver(0, 1);

    EXPECT_EQ(currentCommandStreamReceiver, executionEnvironment->commandStreamReceivers[0][1].get());
    EXPECT_EQ(2u, executionEnvironment->commandStreamReceivers[0].size());
    EXPECT_EQ(nullptr, executionEnvironment->commandStreamReceivers[0][0].get());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsReceivesCorrectInputParams) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.initAubCenter(true, "test.aub", CommandStreamReceiverType::CSR_AUB);
    EXPECT_TRUE(executionEnvironment.initAubCenterCalled);
    EXPECT_TRUE(executionEnvironment.localMemoryEnabledReceived);
    EXPECT_STREQ(executionEnvironment.aubFileNameReceived.c_str(), "test.aub");
}

TEST(ExecutionEnvironment, givenUseAubStreamFalseWhenGetAubManagerIsCalledThenReturnNull) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);

    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto aubManager = executionEnvironment->aubCenter->getAubManager();
    EXPECT_EQ(nullptr, aubManager);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto currentAubCenter = executionEnvironment->aubCenter.get();
    EXPECT_NE(nullptr, currentAubCenter);
    auto currentAubStreamProvider = currentAubCenter->getStreamProvider();
    EXPECT_NE(nullptr, currentAubStreamProvider);
    auto currentAubFileStream = currentAubStreamProvider->getStream();
    EXPECT_NE(nullptr, currentAubFileStream);
    executionEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(currentAubCenter, executionEnvironment->aubCenter.get());
    EXPECT_EQ(currentAubStreamProvider, executionEnvironment->aubCenter->getStreamProvider());
    EXPECT_EQ(currentAubFileStream, executionEnvironment->aubCenter->getStreamProvider()->getStream());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenLocalMemorySupportedInMemoryManagerHasCorrectValue) {
    const HardwareInfo *hwInfo = platformDevices[0];
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->initializeCommandStreamReceiver(0, 0);
    auto enableLocalMemory = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo);
    executionEnvironment->initializeMemoryManager();
    EXPECT_EQ(enableLocalMemory, executionEnvironment->memoryManager->isLocalMemorySupported());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeCommandStreamReceiver(0, 0);
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}
static_assert(sizeof(ExecutionEnvironment) == sizeof(std::vector<std::unique_ptr<CommandStreamReceiver>>) +
                                                  sizeof(std::unique_ptr<CommandStreamReceiver>) +
                                                  sizeof(std::mutex) +
                                                  sizeof(std::unique_ptr<HardwareInfo>) +
                                                  (is64bit ? 80 : 44),
              "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MockExecutionEnvironment : ExecutionEnvironment {
        using ExecutionEnvironment::gmmHelper;
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 8> {
        GmmHelperMock(uint32_t &destructorId, const HardwareInfo *hwInfo) : DestructorCounted(destructorId, hwInfo) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 7> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 6> {
        MemoryMangerMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 5> {
        AubCenterMock(uint32_t &destructorId) : DestructorCounted(destructorId, platformDevices[0], false, "", CommandStreamReceiverType::CSR_AUB) {}
    };
    struct CommandStreamReceiverMock : public DestructorCounted<MockCommandStreamReceiver, 4> {
        CommandStreamReceiverMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {}
    };
    struct SpecialCommandStreamReceiverMock : public DestructorCounted<MockCommandStreamReceiver, 3> {
        SpecialCommandStreamReceiverMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {}
    };
    struct BuiltinsMock : public DestructorCounted<BuiltIns, 2> {
        BuiltinsMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct CompilerInterfaceMock : public DestructorCounted<CompilerInterface, 1> {
        CompilerInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct SourceLevelDebuggerMock : public DestructorCounted<SourceLevelDebugger, 0> {
        SourceLevelDebuggerMock(uint32_t &destructorId) : DestructorCounted(destructorId, nullptr) {}
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setHwInfo(*platformDevices);
    executionEnvironment->commandStreamReceivers.resize(1);
    executionEnvironment->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, platformDevices[0]);
    executionEnvironment->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId, *executionEnvironment);
    executionEnvironment->aubCenter = std::make_unique<AubCenterMock>(destructorId);
    executionEnvironment->commandStreamReceivers[0].push_back(std::make_unique<CommandStreamReceiverMock>(destructorId, *executionEnvironment));
    executionEnvironment->specialCommandStreamReceiver = std::make_unique<SpecialCommandStreamReceiverMock>(destructorId, *executionEnvironment);
    executionEnvironment->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->sourceLevelDebugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(9u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleDevicesWhenTheyAreCreatedTheyAllReuseTheSameMemoryManagerAndCommandStreamReceiver) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    std::unique_ptr<MockDevice> device(Device::create<MockDevice>(executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<MockDevice> device2(Device::create<MockDevice>(executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

typedef ::testing::Test ExecutionEnvironmentHw;

HWTEST_F(ExecutionEnvironmentHw, givenHwHelperInputWhenInitializingCsrThenCreatePageTableManagerIfAllowed) {
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(&localHwInfo);
    executionEnvironment.initializeCommandStreamReceiver(0, 0);
    auto csr0 = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[0][0].get());
    EXPECT_FALSE(csr0->createPageTableManagerCalled);

    auto hwInfo = executionEnvironment.getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo->capabilityTable.ftrRenderCompressedImages = false;
    executionEnvironment.initializeCommandStreamReceiver(1, 0);
    auto csr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[1][0].get());
    EXPECT_EQ(UnitTestHelper<FamilyType>::isPageTableManagerSupported(*hwInfo), csr1->createPageTableManagerCalled);

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo->capabilityTable.ftrRenderCompressedImages = true;
    executionEnvironment.initializeCommandStreamReceiver(2, 0);
    auto csr2 = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[2][0].get());
    EXPECT_EQ(UnitTestHelper<FamilyType>::isPageTableManagerSupported(*hwInfo), csr2->createPageTableManagerCalled);
}

TEST(ExecutionEnvironment, whenSpecialCsrNotExistThenReturnNullSpecialEngineControl) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeCommandStreamReceiver(0, 0);
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
    auto engineControl = executionEnvironment->getEngineControlForSpecialCsr();
    EXPECT_EQ(nullptr, engineControl);
}

TEST(ExecutionEnvironment, whenSpecialCsrExistsThenReturnSpecialEngineControl) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeCommandStreamReceiver(0, 0);
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);

    executionEnvironment->specialCommandStreamReceiver.reset(createCommandStream(*executionEnvironment));
    auto engineType = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0];
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(executionEnvironment->specialCommandStreamReceiver.get(),
                                                                                     engineType, 1,
                                                                                     PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    executionEnvironment->specialCommandStreamReceiver->setupContext(*osContext);

    auto engineControl = executionEnvironment->getEngineControlForSpecialCsr();
    ASSERT_NE(nullptr, engineControl);
    EXPECT_EQ(executionEnvironment->specialCommandStreamReceiver.get(), engineControl->commandStreamReceiver);
}

TEST(ExecutionEnvironment, givenUnproperSetCsrFlagValueWhenInitializingMemoryManagerThenCreateDefaultMemoryManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(10);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(*platformDevices);
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

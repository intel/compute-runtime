/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
#include "runtime/built_ins/built_ins.h"
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
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/utilities/destructor_counted.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"

using namespace OCLRT;

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

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesMemoryManagerInExecutionEnvironment) {
    Platform platform;
    auto executionEnvironment = platform.peekExecutionEnvironment();
    platform.initialize();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenDeviceWhenItIsDestroyedThenMemoryManagerIsStillAvailable) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->incRefInternal();
    std::unique_ptr<Device> device(Device::create<OCLRT::Device>(nullptr, executionEnvironment.get(), 0u));
    device.reset(nullptr);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeCommandStreamReceiverIsCalledThenItIsInitalized) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0, 0);
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0][0]);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeIsCalledWithDifferentDeviceIndexesThenInternalStorageIsResized) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    EXPECT_EQ(0u, executionEnvironment->commandStreamReceivers.size());
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0, 0);
    EXPECT_EQ(1u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0][0]);
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 1, 0);
    EXPECT_EQ(2u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[1][0]);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeIsCalledMultipleTimesForTheSameIndexThenCommandStreamReceiverIsReused) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    EXPECT_EQ(0u, executionEnvironment->commandStreamReceivers.size());
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0, 1);

    auto currentCommandStreamReceiver = executionEnvironment->commandStreamReceivers[0][1].get();

    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0, 1);

    EXPECT_EQ(currentCommandStreamReceiver, executionEnvironment->commandStreamReceivers[0][1].get());
    EXPECT_EQ(2u, executionEnvironment->commandStreamReceivers[0].size());
    EXPECT_EQ(nullptr, executionEnvironment->commandStreamReceivers[0][0].get());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsReceivesCorrectInputParams) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initAubCenter(platformDevices[0], true, "test.aub");
    EXPECT_TRUE(executionEnvironment.initAubCenterCalled);
    EXPECT_TRUE(executionEnvironment.localMemoryEnabledReceived);
    EXPECT_STREQ(executionEnvironment.aubFileNameReceived.c_str(), "test.aub");
}

TEST(ExecutionEnvironment, givenUseAubStreamFalseWhenGetAubManagerIsCalledThenReturnNull) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initAubCenter(platformDevices[0], false, "");
    auto aubManager = executionEnvironment.aubCenter->getAubManager();
    EXPECT_EQ(nullptr, aubManager);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initAubCenter(platformDevices[0], false, "");
    auto currentAubCenter = executionEnvironment.aubCenter.get();
    EXPECT_NE(nullptr, currentAubCenter);
    auto currentAubStreamProvider = currentAubCenter->getStreamProvider();
    EXPECT_NE(nullptr, currentAubStreamProvider);
    auto currentAubFileStream = currentAubStreamProvider->getStream();
    EXPECT_NE(nullptr, currentAubFileStream);
    executionEnvironment.initAubCenter(platformDevices[0], false, "");
    EXPECT_EQ(currentAubCenter, executionEnvironment.aubCenter.get());
    EXPECT_EQ(currentAubStreamProvider, executionEnvironment.aubCenter->getStreamProvider());
    EXPECT_EQ(currentAubFileStream, executionEnvironment.aubCenter->getStreamProvider()->getStream());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenLocalMemorySupportedInMemoryManagerHasCorrectValue) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0, 0);
    executionEnvironment->initializeMemoryManager(false, device->getEnableLocalMemory(), 0, 0);
    EXPECT_EQ(device->getEnableLocalMemory(), executionEnvironment->memoryManager->isLocalMemorySupported());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0, 0);
    executionEnvironment->initializeMemoryManager(false, false, 0, 0);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}
static_assert(sizeof(ExecutionEnvironment) == sizeof(std::vector<std::unique_ptr<CommandStreamReceiver>>) + sizeof(std::mutex) + (is64bit ? 80 : 44), "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MockExecutionEnvironment : ExecutionEnvironment {
        using ExecutionEnvironment::gmmHelper;
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 7> {
        GmmHelperMock(uint32_t &destructorId, const HardwareInfo *hwInfo) : DestructorCounted(destructorId, hwInfo) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 6> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 5> {
        MemoryMangerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 4> {
        AubCenterMock(uint32_t &destructorId) : DestructorCounted(destructorId, platformDevices[0], false, "") {}
    };
    struct CommandStreamReceiverMock : public DestructorCounted<MockCommandStreamReceiver, 3> {
        CommandStreamReceiverMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {}
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
    executionEnvironment->commandStreamReceivers.resize(1);
    executionEnvironment->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, platformDevices[0]);
    executionEnvironment->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId);
    executionEnvironment->aubCenter = std::make_unique<AubCenterMock>(destructorId);
    executionEnvironment->commandStreamReceivers[0].push_back(std::make_unique<CommandStreamReceiverMock>(destructorId, *executionEnvironment));
    executionEnvironment->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->sourceLevelDebugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(8u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleDevicesWhenTheyAreCreatedTheyAllReuseTheSameMemoryManagerAndCommandStreamReceiver) {
    auto executionEnvironment = new ExecutionEnvironment;
    std::unique_ptr<MockDevice> device(Device::create<OCLRT::MockDevice>(nullptr, executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<MockDevice> device2(Device::create<OCLRT::MockDevice>(nullptr, executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

typedef ::testing::Test ExecutionEnvironmentHw;

HWTEST_F(ExecutionEnvironmentHw, givenHwHelperInputWhenInitializingCsrThenCreatePageTableManagerIfAllowed) {
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo, 0, 0);
    auto csr0 = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[0][0].get());
    EXPECT_FALSE(csr0->createPageTableManagerCalled);

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo, 1, 0);
    auto csr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[1][0].get());
    EXPECT_EQ(UnitTestHelper<FamilyType>::isPageTableManagerSupported(localHwInfo), csr1->createPageTableManagerCalled);

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo, 2, 0);
    auto csr2 = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[2][0].get());
    EXPECT_EQ(UnitTestHelper<FamilyType>::isPageTableManagerSupported(localHwInfo), csr2->createPageTableManagerCalled);
}

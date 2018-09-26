/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/aub_center.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

#include "test.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/utilities/destructor_counted.h"

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
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0u].get());
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
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0u);
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0u]);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeIsCalledWithDifferentDeviceIndexesThenInternalStorageIsResized) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    EXPECT_EQ(0u, executionEnvironment->commandStreamReceivers.size());
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0u);
    EXPECT_EQ(1u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0u]);
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 1u);
    EXPECT_EQ(2u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[1u]);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeIsCalledMultipleTimesForTheSameIndexThenCommandStreamReceiverIsReused) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    EXPECT_EQ(0u, executionEnvironment->commandStreamReceivers.size());
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 1u);

    auto currentCommandStreamReceiver = executionEnvironment->commandStreamReceivers[1u].get();

    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 1u);

    EXPECT_EQ(currentCommandStreamReceiver, executionEnvironment->commandStreamReceivers[1u].get());
    EXPECT_EQ(2u, executionEnvironment->commandStreamReceivers.size());
    EXPECT_EQ(nullptr, executionEnvironment->commandStreamReceivers[0u].get());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initAubCenter();
    auto currentAubCenter = executionEnvironment.aubCenter.get();
    EXPECT_NE(nullptr, currentAubCenter);
    auto currentAubStreamProvider = currentAubCenter->getStreamProvider();
    EXPECT_NE(nullptr, currentAubStreamProvider);
    auto currentAubFileStream = currentAubStreamProvider->getStream();
    EXPECT_NE(nullptr, currentAubFileStream);
    executionEnvironment.initAubCenter();
    EXPECT_EQ(currentAubCenter, executionEnvironment.aubCenter.get());
    EXPECT_EQ(currentAubStreamProvider, executionEnvironment.aubCenter->getStreamProvider());
    EXPECT_EQ(currentAubFileStream, executionEnvironment.aubCenter->getStreamProvider()->getStream());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->initializeCommandStreamReceiver(platformDevices[0], 0u);
    executionEnvironment->initializeMemoryManager(false, false, 0u);
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
    struct MemoryMangerMock : public DestructorCounted<OsAgnosticMemoryManager, 5> {
        MemoryMangerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 4> {
        AubCenterMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct CommandStreamReceiverMock : public DestructorCounted<MockCommandStreamReceiver, 3> {
        CommandStreamReceiverMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
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
    executionEnvironment->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, platformDevices[0]);
    executionEnvironment->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId);
    executionEnvironment->aubCenter = std::make_unique<AubCenterMock>(destructorId);
    executionEnvironment->commandStreamReceivers.push_back(std::make_unique<CommandStreamReceiverMock>(destructorId));
    executionEnvironment->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->sourceLevelDebugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(8u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleDevicesWhenTheyAreCreatedTheyAllReuseTheSameMemoryManagerAndCommandStreamReceiver) {
    auto executionEnvironment = new ExecutionEnvironment;
    std::unique_ptr<Device> device(Device::create<OCLRT::Device>(nullptr, executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<Device> device2(Device::create<OCLRT::Device>(nullptr, executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

typedef ::testing::Test ExecutionEnvironmentHw;

HWTEST_F(ExecutionEnvironmentHw, givenExecutionEnvironmentWhenCommandStreamReceiverIsInitializedForCompressedBuffersThenCreatePageTableManagerIsCalled) {
    ExecutionEnvironment executionEnvironment;
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo, 0u);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[0u].get());
    ASSERT_NE(nullptr, csr);
    EXPECT_TRUE(csr->createPageTableManagerCalled);
}

HWTEST_F(ExecutionEnvironmentHw, givenExecutionEnvironmentWhenCommandStreamReceiverIsInitializedForCompressedImagesThenCreatePageTableManagerIsCalled) {
    ExecutionEnvironment executionEnvironment;
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    executionEnvironment.initializeCommandStreamReceiver(&localHwInfo, 0u);
    EXPECT_NE(nullptr, executionEnvironment.commandStreamReceivers[0]);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(executionEnvironment.commandStreamReceivers[0u].get());
    ASSERT_NE(nullptr, csr);
    EXPECT_TRUE(csr->createPageTableManagerCalled);
}

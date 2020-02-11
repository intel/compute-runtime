/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/preemption.h"
#include "core/compiler_interface/compiler_interface.h"
#include "core/device/device.h"
#include "core/execution_environment/execution_environment.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/hw_helper.h"
#include "core/os_interface/os_interface.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/utilities/destructor_counted.h"
#include "runtime/aub/aub_center.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "test.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_memory_operations_handler.h"

using namespace NEO;

TEST(ExecutionEnvironment, givenDefaultConstructorWhenItIsCalledThenExecutionEnvironmentHasInitialRefCountZero) {
    ExecutionEnvironment environment;
    EXPECT_EQ(0, environment.getRefInternalCount());
    EXPECT_EQ(0, environment.getRefApiCount());
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsConstructedThenItCretesExecutionEnvironmentWithOneRefCountInternal) {
    auto executionEnvironment = new ExecutionEnvironment();
    EXPECT_EQ(0, executionEnvironment->getRefInternalCount());

    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    EXPECT_EQ(executionEnvironment, platform->peekExecutionEnvironment());
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenPlatformAndExecutionEnvironmentWithRefCountsWhenPlatformIsDestroyedThenExecutionEnvironmentIsNotDeleted) {
    auto executionEnvironment = new ExecutionEnvironment();
    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    executionEnvironment->incRefInternal();
    platform.reset();
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());
    executionEnvironment->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsInitializedAndCreatesDevicesThenThoseDevicesAddRefcountsToExecutionEnvironment) {
    auto executionEnvironment = new ExecutionEnvironment();
    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));

    auto expectedRefCounts = executionEnvironment->getRefInternalCount();
    size_t numRootDevices;
    getDevices(numRootDevices, *executionEnvironment);
    platform->initialize(1, 0);
    EXPECT_LT(0u, platform->getDevice(0)->getNumAvailableDevices());
    if (platform->getDevice(0)->getNumAvailableDevices() > 1) {
        expectedRefCounts++;
    }
    expectedRefCounts += platform->getDevice(0)->getNumAvailableDevices();
    EXPECT_EQ(expectedRefCounts, executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenDeviceThatHaveRefferencesAfterPlatformIsDestroyedThenDeviceIsStillUsable) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(1);
    auto executionEnvironment = new ExecutionEnvironment();
    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    size_t numRootDevices;
    getDevices(numRootDevices, *executionEnvironment);
    platform->initialize(1, 0);
    auto device = platform->getClDevice(0);
    EXPECT_EQ(1, device->getRefInternalCount());
    device->incRefInternal();
    platform.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());

    device->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesMemoryManagerInExecutionEnvironment) {
    auto executionEnvironment = new ExecutionEnvironment();
    Platform platform(*executionEnvironment);
    size_t numRootDevices;
    getDevices(numRootDevices, *executionEnvironment);
    platform.initialize(1, 0);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenDeviceWhenItIsDestroyedThenMemoryManagerIsStillAvailable) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<Device> device(Device::create<RootDevice>(executionEnvironment, 0u));
    device.reset(nullptr);
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsReceivesCorrectInputParams) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->initAubCenter(true, "test.aub", CommandStreamReceiverType::CSR_AUB);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_TRUE(rootDeviceEnvironment->localMemoryEnabledReceived);
    EXPECT_STREQ(rootDeviceEnvironment->aubFileNameReceived.c_str(), "test.aub");
}

TEST(RootDeviceEnvironment, givenUseAubStreamFalseWhenGetAubManagerIsCalledThenReturnNull) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);

    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto aubManager = rootDeviceEnvironment->aubCenter->getAubManager();
    EXPECT_EQ(nullptr, aubManager);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto currentAubCenter = rootDeviceEnvironment->aubCenter.get();
    EXPECT_NE(nullptr, currentAubCenter);
    auto currentAubStreamProvider = currentAubCenter->getStreamProvider();
    EXPECT_NE(nullptr, currentAubStreamProvider);
    auto currentAubFileStream = currentAubStreamProvider->getStream();
    EXPECT_NE(nullptr, currentAubFileStream);
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(currentAubCenter, rootDeviceEnvironment->aubCenter.get());
    EXPECT_EQ(currentAubStreamProvider, rootDeviceEnvironment->aubCenter->getStreamProvider());
    EXPECT_EQ(currentAubFileStream, rootDeviceEnvironment->aubCenter->getStreamProvider()->getStream());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenLocalMemorySupportedInMemoryManagerHasCorrectValue) {
    const HardwareInfo *hwInfo = platformDevices[0];
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));
    auto executionEnvironment = device->getExecutionEnvironment();
    auto enableLocalMemory = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo);
    executionEnvironment->initializeMemoryManager();
    EXPECT_EQ(enableLocalMemory, executionEnvironment->memoryManager->isLocalMemorySupported());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}
static_assert(sizeof(ExecutionEnvironment) == sizeof(std::mutex) +
                                                  sizeof(std::unique_ptr<HardwareInfo>) +
                                                  sizeof(std::vector<RootDeviceEnvironment>) +
                                                  (is64bit ? 64 : 36),
              "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MockExecutionEnvironment : ExecutionEnvironment {
        using ExecutionEnvironment::gmmHelper;
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 7> {
        GmmHelperMock(uint32_t &destructorId, const HardwareInfo *hwInfo) : DestructorCounted(destructorId, nullptr, hwInfo) {}
    };
    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 6> {
        MemoryMangerMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 5> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryOperationsHandlerMock : public DestructorCounted<MockMemoryOperationsHandler, 4> {
        MemoryOperationsHandlerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 3> {
        AubCenterMock(uint32_t &destructorId) : DestructorCounted(destructorId, platformDevices[0], false, "", CommandStreamReceiverType::CSR_AUB) {}
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
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, platformDevices[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MemoryOperationsHandlerMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId, *executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::make_unique<AubCenterMock>(destructorId);
    executionEnvironment->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->debugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(8u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleRootDevicesWhenTheyAreCreatedTheyAllReuseTheSameMemoryManager) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    std::unique_ptr<MockDevice> device(Device::create<MockDevice>(executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<MockDevice> device2(Device::create<MockDevice>(executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

TEST(ExecutionEnvironment, givenUnproperSetCsrFlagValueWhenInitializingMemoryManagerThenCreateDefaultMemoryManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(10);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(*platformDevices);
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, whenCalculateMaxOsContexCountThenGlobalVariableHasProperValue) {
    VariableBackup<uint32_t> osContextCountBackup(&MemoryManager::maxOsContextCount);
    MockExecutionEnvironment executionEnvironment;
    uint32_t numRootDevices = 17u;
    auto &hwHelper = HwHelper::get(executionEnvironment.getHardwareInfo()->platform.eRenderCoreFamily);
    auto osContextCount = hwHelper.getGpgpuEngineInstances().size();
    auto subDevicesCount = HwHelper::getSubDevicesCount(executionEnvironment.getHardwareInfo());
    bool hasRootCsr = subDevicesCount > 1;

    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);
    executionEnvironment.calculateMaxOsContextCount();

    auto expectedOsContextCount = numRootDevices * osContextCount * subDevicesCount + hasRootCsr;
    EXPECT_EQ(expectedOsContextCount, MemoryManager::maxOsContextCount);
}
TEST(RootDeviceEnvironment, whenGetHardwareInfoIsCalledThenHardwareInfoIsTakenFromExecutionEnvironment) {
    ExecutionEnvironment executionEnvironment;
    HardwareInfo hwInfo = {};
    executionEnvironment.setHwInfo(&hwInfo);
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    EXPECT_EQ(rootDeviceEnvironment.getHardwareInfo(), executionEnvironment.getHardwareInfo());
}

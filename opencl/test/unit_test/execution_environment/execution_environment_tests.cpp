/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/aub/aub_center.h"
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/destructor_counted.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_async_event_handler.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

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

TEST(ExecutionEnvironment, WhenCreatingDevicesThenThoseDevicesAddRefcountsToExecutionEnvironment) {
    auto executionEnvironment = new ExecutionEnvironment();

    auto expectedRefCounts = executionEnvironment->getRefInternalCount();
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_LE(0u, devices[0]->getNumSubDevices());
    if (devices[0]->getNumSubDevices() > 1) {
        expectedRefCounts++;
    }
    expectedRefCounts += std::max(devices[0]->getNumSubDevices(), 1u);
    EXPECT_EQ(expectedRefCounts, executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenDeviceThatHaveRefferencesAfterPlatformIsDestroyedThenDeviceIsStillUsable) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(1);
    auto executionEnvironment = new ExecutionEnvironment();
    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    platform->initialize(DeviceFactory::createDevices(*executionEnvironment));
    auto device = platform->getClDevice(0);
    EXPECT_EQ(1, device->getRefInternalCount());
    device->incRefInternal();
    platform.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());

    int32_t expectedRefCount = 1 + device->getNumSubDevices();

    EXPECT_EQ(expectedRefCount, executionEnvironment->getRefInternalCount());

    device->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesMemoryManagerInExecutionEnvironment) {
    auto executionEnvironment = new ExecutionEnvironment();
    Platform platform(*executionEnvironment);
    prepareDeviceEnvironments(*executionEnvironment);
    platform.initialize(DeviceFactory::createDevices(*executionEnvironment));
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, givenMemoryManagerIsNotInitializedInExecutionEnvironmentWhenCreatingDevicesThenEmptyDeviceVectorIsReturned) {
    class FailedInitializeMemoryManagerExecutionEnvironment : public MockExecutionEnvironment {
        bool initializeMemoryManager() override { return false; }
    };

    auto executionEnvironment = std::make_unique<FailedInitializeMemoryManagerExecutionEnvironment>();
    prepareDeviceEnvironments(*executionEnvironment);
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_TRUE(devices.empty());
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
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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
    const HardwareInfo *hwInfo = defaultHwInfo.get();
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));
    auto executionEnvironment = device->getExecutionEnvironment();
    auto enableLocalMemory = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo);
    executionEnvironment->initializeMemoryManager();
    EXPECT_EQ(enableLocalMemory, executionEnvironment->memoryManager->isLocalMemorySupported(device->getRootDeviceIndex()));
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetWhenInitializeDirectSubmissionControllerThenNotNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmissionController.set(1);

    auto controller = platform()->peekExecutionEnvironment()->initializeDirectSubmissionController();

    EXPECT_NE(controller, nullptr);
}

TEST(ExecutionEnvironment, givenSetCsrFlagSetWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(1);

    auto controller = platform()->peekExecutionEnvironment()->initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetZeroWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmissionController.set(0);

    auto controller = platform()->peekExecutionEnvironment()->initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}
static_assert(sizeof(ExecutionEnvironment) == sizeof(std::unique_ptr<HardwareInfo>) +
                                                  sizeof(std::vector<RootDeviceEnvironment>) +
                                                  sizeof(std::unique_ptr<OsEnvironment>) +
                                                  sizeof(std::unique_ptr<DirectSubmissionController>) +
                                                  sizeof(bool) +
                                                  (is64bit ? 23 : 15),
              "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 8> {
        MemoryMangerMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {
            callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
            callBasePopulateOsHandles = false;
        }
    };
    struct DirectSubmissionControllerMock : public DestructorCounted<DirectSubmissionController, 7> {
        DirectSubmissionControllerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 6> {
        GmmHelperMock(uint32_t &destructorId, const HardwareInfo *hwInfo) : DestructorCounted(destructorId, nullptr, hwInfo) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 5> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryOperationsHandlerMock : public DestructorCounted<MockMemoryOperationsHandler, 4> {
        MemoryOperationsHandlerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 3> {
        AubCenterMock(uint32_t &destructorId, const GmmHelper &gmmHelper) : DestructorCounted(destructorId, defaultHwInfo.get(), gmmHelper, false, "", CommandStreamReceiverType::CSR_AUB) {}
    };
    struct CompilerInterfaceMock : public DestructorCounted<CompilerInterface, 2> {
        CompilerInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct BuiltinsMock : public DestructorCounted<BuiltIns, 1> {
        BuiltinsMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct SourceLevelDebuggerMock : public DestructorCounted<SourceLevelDebugger, 0> {
        SourceLevelDebuggerMock(uint32_t &destructorId) : DestructorCounted(destructorId, nullptr) {}
    };

    auto gmmHelper = new GmmHelperMock(destructorId, defaultHwInfo.get());

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->gmmHelper = std::unique_ptr<GmmHelperMock>(gmmHelper);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MemoryOperationsHandlerMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId, *executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::make_unique<AubCenterMock>(destructorId, *gmmHelper);
    executionEnvironment->rootDeviceEnvironments[0]->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->debugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);
    executionEnvironment->directSubmissionController = std::make_unique<DirectSubmissionControllerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(9u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleRootDevicesWhenTheyAreCreatedThenReuseMemoryManager) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    std::unique_ptr<MockDevice> device(Device::create<MockDevice>(executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<MockDevice> device2(Device::create<MockDevice>(executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

uint64_t isDriverAvaliableCounter = 0u;

class DriverModelMock : public DriverModel {
  public:
    DriverModelMock(DriverModelType driverModelType) : DriverModel(driverModelType) {
    }

    bool isDriverAvaliable() override {
        isDriverAvaliableCounter++;
        return true;
    }
    void setGmmInputArgs(void *args) override {
    }

    uint32_t getDeviceHandle() const override {
        return 0;
    }

    PhysicalDevicePciBusInfo getPciBusInfo() const override {
        return {};
    }
    PhyicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return {};
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    bool isGpuHangDetected(OsContext &osContext) override {
        return false;
    }
};

TEST(ExecutionEnvironment, givenRootDeviceWhenPrepareForCleanupThenIsDriverAvaliableIsCalled) {
    VariableBackup<uint64_t> varBackup = &isDriverAvaliableCounter;
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();

    std::unique_ptr<OSInterface> osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::make_unique<DriverModelMock>(DriverModelType::UNKNOWN));

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::move(osInterface);

    executionEnvironment->prepareForCleanup();

    EXPECT_EQ(1u, isDriverAvaliableCounter);
}

TEST(ExecutionEnvironment, givenUnproperSetCsrFlagValueWhenInitializingMemoryManagerThenCreateDefaultMemoryManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(10);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, whenCalculateMaxOsContexCountThenGlobalVariableHasProperValue) {
    DebugManagerStateRestore restore;
    VariableBackup<uint32_t> osContextCountBackup(&MemoryManager::maxOsContextCount, 0);
    uint32_t numRootDevices = 17u;
    uint32_t expectedOsContextCount = 0u;
    uint32_t expectedOsContextCountForCcs = 0u;

    {
        DebugManager.flags.EngineInstancedSubDevices.set(false);
        MockExecutionEnvironment executionEnvironment(nullptr, true, numRootDevices);

        for (const auto &rootDeviceEnvironment : executionEnvironment.rootDeviceEnvironments) {
            auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
            auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
            auto osContextCount = hwHelper.getGpgpuEngineInstances(*hwInfo).size();
            auto subDevicesCount = HwHelper::getSubDevicesCount(hwInfo);
            bool hasRootCsr = subDevicesCount > 1;
            auto ccsCount = hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

            expectedOsContextCount += static_cast<uint32_t>(osContextCount * subDevicesCount + hasRootCsr);

            if (ccsCount > 1) {
                expectedOsContextCountForCcs += ccsCount * subDevicesCount;
            }
        }

        EXPECT_EQ(expectedOsContextCount, MemoryManager::maxOsContextCount);
    }

    {
        DebugManager.flags.EngineInstancedSubDevices.set(true);
        MockExecutionEnvironment executionEnvironment(nullptr, true, numRootDevices);

        EXPECT_EQ(expectedOsContextCount + expectedOsContextCountForCcs, MemoryManager::maxOsContextCount);
    }
}

TEST(ClExecutionEnvironment, WhenExecutionEnvironmentIsDeletedThenAsyncEventHandlerThreadIsDestroyed) {
    auto executionEnvironment = new MockClExecutionEnvironment();
    MockHandler *mockAsyncHandler = new MockHandler();

    executionEnvironment->asyncEventsHandler.reset(mockAsyncHandler);
    EXPECT_EQ(mockAsyncHandler, executionEnvironment->getAsyncEventsHandler());

    mockAsyncHandler->openThread();
    delete executionEnvironment;
    EXPECT_TRUE(MockAsyncEventHandlerGlobals::destructorCalled);
}

TEST(ClExecutionEnvironment, WhenExecutionEnvironmentIsCreatedThenAsyncEventHandlerIsCreated) {
    auto executionEnvironment = std::make_unique<ClExecutionEnvironment>();
    EXPECT_NE(nullptr, executionEnvironment->getAsyncEventsHandler());
}

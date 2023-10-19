/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/execution_environment/execution_environment_tests.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/destructor_counted.h"

using namespace NEO;

TEST(ExecutionEnvironment, givenDefaultConstructorWhenItIsCalledThenExecutionEnvironmentHasInitialRefCountZero) {
    ExecutionEnvironment environment;
    EXPECT_EQ(0, environment.getRefInternalCount());
    EXPECT_EQ(0, environment.getRefApiCount());
}

TEST(ExecutionEnvironment, givenDefaultConstructoWhenItIsCalledAndReturnSubDevicesAsApiDevicesIsSetTrueThenSubDevicesAsDevicesIsTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ReturnSubDevicesAsApiDevices.set(1);
    MockExecutionEnvironment environment;
    environment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    environment.setDeviceHierarchy(environment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    EXPECT_TRUE(environment.isExposingSubDevicesAsDevices());
}

TEST(ExecutionEnvironment, givenDefaultConstructoWhenItIsCalledAndReturnSubDevicesAsApiDevicesIsSetFalseThenSubDevicesAsDevicesIsFalse) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ReturnSubDevicesAsApiDevices.set(0);
    MockExecutionEnvironment environment;
    environment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    environment.setDeviceHierarchy(environment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    EXPECT_FALSE(environment.isExposingSubDevicesAsDevices());
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
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.incRefInternal();
    executionEnvironment.initializeMemoryManager();
    std::unique_ptr<Device> device(Device::create<RootDevice>(&executionEnvironment, 0u));
    device.reset(nullptr);
    EXPECT_NE(nullptr, executionEnvironment.memoryManager);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsReceivesCorrectInputParams) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->initAubCenter(true, "test.aub", CommandStreamReceiverType::CSR_AUB);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_TRUE(rootDeviceEnvironment->localMemoryEnabledReceived);
    EXPECT_STREQ(rootDeviceEnvironment->aubFileNameReceived.c_str(), "test.aub");
}

TEST(RootDeviceEnvironment, whenCreatingRootDeviceEnvironmentThenCreateOsAgnosticOsTime) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto profilingTimerResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    EXPECT_EQ(nullptr, rootDeviceEnvironment->osTime.get());
    rootDeviceEnvironment->initOsTime();

    uint64_t ts = 123;
    EXPECT_TRUE(rootDeviceEnvironment->osTime->getCpuTime(&ts));
    EXPECT_EQ(0u, ts);

    EXPECT_EQ(0u, rootDeviceEnvironment->osTime->getHostTimerResolution());
    EXPECT_EQ(0u, rootDeviceEnvironment->osTime->getCpuRawTimestamp());

    TimeStampData tsData{1, 2};
    EXPECT_TRUE(rootDeviceEnvironment->osTime->getCpuGpuTime(&tsData));
    EXPECT_EQ(0u, tsData.cpuTimeinNS);
    EXPECT_EQ(0u, tsData.gpuTimeStamp);

    EXPECT_EQ(profilingTimerResolution, rootDeviceEnvironment->osTime->getDynamicDeviceTimerResolution(*defaultHwInfo));
    EXPECT_EQ(static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(*defaultHwInfo)), rootDeviceEnvironment->osTime->getDynamicDeviceTimerClock(*defaultHwInfo));
}

TEST(RootDeviceEnvironment, givenUseAubStreamFalseWhenGetAubManagerIsCalledThenReturnNull) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);

    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), false, 1u};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto aubManager = rootDeviceEnvironment->aubCenter->getAubManager();
    EXPECT_EQ(nullptr, aubManager);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), false, 1u};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
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

TEST(RootDeviceEnvironment, givenRootExecutionEnvironmentWhenGetAssertHandlerIsCalledThenItIsInitalizedOnce) {
    const HardwareInfo *hwInfo = defaultHwInfo.get();
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));
    auto executionEnvironment = device->getExecutionEnvironment();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto assertHandler = rootDeviceEnvironment->getAssertHandler(device.get());

    EXPECT_NE(nullptr, assertHandler);
    EXPECT_EQ(assertHandler, rootDeviceEnvironment->getAssertHandler(device.get()));
}

TEST(RootDeviceEnvironment, givenDefaultHardwareInfoWhenPrepareDeviceEnvironmentsThenFtrRcsNodeIsCorrectSet) {
    MockExecutionEnvironment executionEnvironment;
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->setRcsExposure();
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();
    auto releaseHelper = rootDeviceEnvironment->getReleaseHelper();

    if (releaseHelper) {
        bool shouldRcsBeDisabled = releaseHelper->isRcsExposureDisabled();
        bool isRcsDisabled = hwInfo->featureTable.flags.ftrRcsNode;
        EXPECT_NE(shouldRcsBeDisabled, isRcsDisabled);
    }
}

TEST(RootDeviceEnvironment, givenHardwareInfoAndDebugVariableNodeOrdinalEqualsRcsWhenPrepareDeviceEnvironmentsThenFtrRcsNodeIsTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->setRcsExposure();
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();

    EXPECT_TRUE(hwInfo->featureTable.flags.ftrRcsNode);
}

TEST(RootDeviceEnvironment, givenHardwareInfoAndDebugVariableNodeOrdinalEqualsCccsWhenPrepareDeviceEnvironmentsThenFtrRcsNodeIsTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->setRcsExposure();
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();

    EXPECT_TRUE(hwInfo->featureTable.flags.ftrRcsNode);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenLocalMemorySupportedInMemoryManagerHasCorrectValue) {
    const HardwareInfo *hwInfo = defaultHwInfo.get();
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));
    auto executionEnvironment = device->getExecutionEnvironment();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto enableLocalMemory = gfxCoreHelper.getEnableLocalMemory(*hwInfo);
    executionEnvironment->initializeMemoryManager();
    EXPECT_EQ(enableLocalMemory, executionEnvironment->memoryManager->isLocalMemorySupported(device->getRootDeviceIndex()));
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetWhenInitializeDirectSubmissionControllerThenNotNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmissionController.set(1);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_NE(controller, nullptr);
}

TEST(ExecutionEnvironment, givenSetCsrFlagSetWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(1);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetZeroWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmissionController.set(0);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenNeoCalEnabledWhenCreateExecutionEnvironmentThenSetDebugVariables) {
    const std::unordered_map<std::string, int32_t> config = {
        {"UseKmdMigration", 0},
        {"SplitBcsSize", 256}};

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef());

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"

#undef DECLARE_DEBUG_VARIABLE

    DebugManagerStateRestore restorer;
    DebugManager.flags.NEO_CAL_ENABLED.set(1);
    ExecutionEnvironment exeEnv;

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)      \
    {                                                                                  \
        if constexpr (std::is_same_v<bool, dataType>) {                                \
            if (strcmp(#variableName, "NEO_CAL_ENABLED") == 0) {                       \
                EXPECT_TRUE(DebugManager.flags.variableName.getRef());                 \
            } else {                                                                   \
                EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef());     \
            }                                                                          \
        } else {                                                                       \
            if constexpr (std::is_same_v<int32_t, dataType>) {                         \
                auto it = config.find(#variableName);                                  \
                if (it != config.end()) {                                              \
                    EXPECT_EQ(it->second, DebugManager.flags.variableName.getRef());   \
                } else {                                                               \
                    EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef()); \
                }                                                                      \
            } else {                                                                   \
                EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef());     \
            }                                                                          \
        }                                                                              \
    }

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"

#undef DECLARE_DEBUG_VARIABLE
}

TEST(ExecutionEnvironment, givenEnvVarUsedInCalConfigAlsoSetByAppWhenCreateExecutionEnvironmentThenRespectAppSetting) {
    constexpr int32_t appCommandBufferAlignment = 12345;
    DebugManagerStateRestore restorer;
    DebugManager.flags.NEO_CAL_ENABLED.set(1);
    DebugManager.flags.ForceCommandBufferAlignment.set(appCommandBufferAlignment);
    ExecutionEnvironment exeEnv;

    EXPECT_EQ(DebugManager.flags.ForceCommandBufferAlignment.get(), appCommandBufferAlignment);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment.memoryManager);
}
static_assert(sizeof(ExecutionEnvironment) == sizeof(std::unique_ptr<HardwareInfo>) +
                                                  sizeof(std::vector<RootDeviceEnvironment>) +
                                                  sizeof(std::unique_ptr<OsEnvironment>) +
                                                  sizeof(std::unique_ptr<DirectSubmissionController>) +
                                                  sizeof(std::unordered_map<uint32_t, uint32_t>) +
                                                  2 * sizeof(bool) +
                                                  sizeof(NEO::DebuggingMode) +
                                                  (is64bit ? 18 : 14) +
                                                  sizeof(std::mutex) +
                                                  sizeof(std::unordered_map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t>>),
              "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 7> {
        MemoryMangerMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {
            callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
            callBasePopulateOsHandles = false;
        }
    };
    struct DirectSubmissionControllerMock : public DestructorCounted<DirectSubmissionController, 6> {
        DirectSubmissionControllerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 5> {
        GmmHelperMock(uint32_t &destructorId, const RootDeviceEnvironment &rootDeviceEnvironment) : DestructorCounted(destructorId, rootDeviceEnvironment) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 4> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryOperationsHandlerMock : public DestructorCounted<MockMemoryOperationsHandler, 3> {
        MemoryOperationsHandlerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 2> {
        AubCenterMock(uint32_t &destructorId, const RootDeviceEnvironment &rootDeviceEnvironment) : DestructorCounted(destructorId, rootDeviceEnvironment, false, "", CommandStreamReceiverType::CSR_AUB) {}
    };
    struct CompilerInterfaceMock : public DestructorCounted<CompilerInterface, 1> {
        CompilerInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct BuiltinsMock : public DestructorCounted<BuiltIns, 0> {
        BuiltinsMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, *executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MemoryOperationsHandlerMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId, *executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::make_unique<AubCenterMock>(destructorId, *executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->directSubmissionController = std::make_unique<DirectSubmissionControllerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(8u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleRootDevicesWhenTheyAreCreatedThenReuseMemoryManager) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.incRefInternal();
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    std::unique_ptr<MockDevice> device(Device::create<MockDevice>(&executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<MockDevice> device2(Device::create<MockDevice>(&executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

uint32_t isDriverAvailableCounter = 0u;

class DefaultDriverModelMock : public MockDriverModel {
  public:
    DefaultDriverModelMock(DriverModelType driverModelType) : MockDriverModel(driverModelType) {
    }

    bool isDriverAvailable() override {
        isDriverAvailableCounter++;
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
    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return {};
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    bool isGpuHangDetected(OsContext &osContext) override {
        return false;
    }
};

TEST(ExecutionEnvironment, givenRootDeviceWhenPrepareForCleanupThenIsDriverAvailableIsCalled) {
    VariableBackup<uint32_t> varBackup{&isDriverAvailableCounter, 0u};
    ExecutionEnvironment executionEnvironment{};

    std::unique_ptr<OSInterface> osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::make_unique<DefaultDriverModelMock>(DriverModelType::UNKNOWN));

    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::move(osInterface);

    executionEnvironment.prepareForCleanup();

    EXPECT_EQ(1u, isDriverAvailableCounter);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<DefaultDriverModelMock>(DriverModelType::UNKNOWN));
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
        bool subDevicesAsDevices = executionEnvironment.isExposingSubDevicesAsDevices();
        for (const auto &rootDeviceEnvironment : executionEnvironment.rootDeviceEnvironments) {
            auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
            auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
            auto osContextCount = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment).size();
            auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(subDevicesAsDevices, hwInfo);
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

TEST(ExecutionEnvironment, givenDefaultExecutionEnvironmentSettingsWhenCheckingFP64EmulationThenFP64EmulationIsDisabled) {
    ExecutionEnvironment executionEnvironment{};
    EXPECT_FALSE(executionEnvironment.isFP64EmulationEnabled());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenSettingFP64EmulationEnabledThenFP64EmulationIsEnabled) {
    ExecutionEnvironment executionEnvironment{};
    ASSERT_FALSE(executionEnvironment.isFP64EmulationEnabled());
    executionEnvironment.setFP64EmulationEnabled();
    EXPECT_TRUE(executionEnvironment.isFP64EmulationEnabled());
}

TEST(ExecutionEnvironmentDeviceHierarchy, givenExecutionEnvironmentWithDefaultDeviceHierarchyThenExecutionEnvironmentIsInitializedCorrectly) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    executionEnvironment.setDeviceHierarchy(gfxCoreHelper);
    EXPECT_EQ((strcmp(gfxCoreHelper.getDefaultDeviceHierarchy(), "COMPOSITE") != 0),
              executionEnvironment.isExposingSubDevicesAsDevices());
}

TEST(ExecutionEnvironmentDeviceHierarchy, givenExecutionEnvironmentWithCompositeDeviceHierarchyThenExposeSubDevicesAsDevicesIsFalse) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.setDeviceHierarchy(executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    EXPECT_FALSE(executionEnvironment.isExposingSubDevicesAsDevices());
}

TEST(ExecutionEnvironmentDeviceHierarchy, givenExecutionEnvironmentWithFlatDeviceHierarchyThenExposeSubDevicesAsDevicesIsTrue) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "FLAT"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.setDeviceHierarchy(executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    EXPECT_TRUE(executionEnvironment.isExposingSubDevicesAsDevices());
}

TEST(ExecutionEnvironmentDeviceHierarchy, givenExecutionEnvironmentWithCombinedDeviceHierarchyThenExposeSubDevicesAsDevicesIsTrue) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMBINED"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.setDeviceHierarchy(executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    EXPECT_FALSE(executionEnvironment.isExposingSubDevicesAsDevices());
}

void ExecutionEnvironmentSortTests::SetUp() {
    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        setupOsSpecifcEnvironment(rootDeviceIndex);
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    }
    executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true; // {0,0,2,0}
    executionEnvironment.rootDeviceEnvironments[3]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true; // {0,1,2,1}
}
TEST_F(ExecutionEnvironmentSortTests, givenEnabledPciIdDeviceOrderFlagWhenSortingDevicesThenRootDeviceEnvironmentsAreSortedByPciId) {
    DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

    NEO::PhysicalDevicePciBusInfo expectedBusInfos[numRootDevices] = {{0, 0, 2, 0}, {0, 0, 2, 1}, {0, 1, 2, 1}, {0, 1, 3, 0}, {3, 1, 2, 0}, {3, 1, 2, 1}};

    executionEnvironment.sortNeoDevices();

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto pciBusInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->getPciBusInfo();
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDomain, pciBusInfo.pciDomain);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciBus, pciBusInfo.pciBus);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDevice, pciBusInfo.pciDevice);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciFunction, pciBusInfo.pciFunction);
    }
}

TEST_F(ExecutionEnvironmentSortTests, givenDisabledPciIdDeviceOrderFlagWhenSortingDevicesThenRootDeviceEnvironmentsAreSortedByTypeThenByPciOrder) {
    DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(0);
    NEO::PhysicalDevicePciBusInfo expectedBusInfos[numRootDevices] = {{0, 0, 2, 1}, {0, 1, 3, 0}, {3, 1, 2, 0}, {3, 1, 2, 1}, {0, 0, 2, 0}, {0, 1, 2, 1}};

    executionEnvironment.sortNeoDevices();

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto pciBusInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->getPciBusInfo();
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDomain, pciBusInfo.pciDomain);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciBus, pciBusInfo.pciBus);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDevice, pciBusInfo.pciDevice);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciFunction, pciBusInfo.pciFunction);
    }
}

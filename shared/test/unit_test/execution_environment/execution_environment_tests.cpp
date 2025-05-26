/*
 * Copyright (C) 2018-2025 Intel Corporation
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
#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
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

TEST(ExecutionEnvironment, givenDeviceCreatorWhenIsMicrosecondAdjustmendRequiredByAilThenGetMicrosecondResolutionCalled) {
    auto executionEnvironment = new ExecutionEnvironment();
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.get());
    mockAIL->adjustMicrosecondResolution = true;
    auto device = Device::create<RootDevice>(executionEnvironment, 0u);
    EXPECT_EQ(mockAIL->getMicrosecondResolutionCalledTimes, 1u);
    delete device;
}

TEST(ExecutionEnvironment, givenDeviceCreatorWhenIsMicrosecondAdjustmendRequiredByAilThenMicrosecondResolutionIsSameAsInMock) {
    auto executionEnvironment = new ExecutionEnvironment();
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.get());
    mockAIL->adjustMicrosecondResolution = true;
    uint32_t expectedResolution = 3u;
    mockAIL->mockMicrosecondResolution = expectedResolution;
    auto device = Device::create<RootDevice>(executionEnvironment, 0u);
    EXPECT_EQ(device->getMicrosecondResolution(), expectedResolution);
    delete device;
}
TEST(ExecutionEnvironment, givenDeviceCreatorWhenAilHelperIsNullThenDefaultValueReturned) {
    auto executionEnvironment = new ExecutionEnvironment();
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.reset();
    uint32_t expectedResolution = 1000u;
    auto device = Device::create<RootDevice>(executionEnvironment, 0u);
    EXPECT_EQ(device->getMicrosecondResolution(), expectedResolution);
    delete device;
}

TEST(ExecutionEnvironment, givenDeviceCreatorWhenIsMicrosecondAdjustmendIsNotRequiredByAilThenGetMicrosecondResolutionIsNotCalled) {
    auto executionEnvironment = new ExecutionEnvironment();
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    MockAILConfiguration mockAilConfigurationHelper;
    executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(executionEnvironment->rootDeviceEnvironments[0u]->ailConfiguration.get());
    mockAIL->adjustMicrosecondResolution = false;
    auto device = Device::create<RootDevice>(executionEnvironment, 0u);
    EXPECT_EQ(mockAIL->getMicrosecondResolutionCalledTimes, 0u);
    delete device;
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

class TestAubCenter : public AubCenter {
  public:
    using AubCenter::AubCenter;

    void resetAubManager() {
        aubManager.reset(nullptr);
    }
};
TEST(RootDeviceEnvironment, givenNullptrAubManagerWhenInitializeAubCenterIsCalledThenMessErrorIsPrintedAndAbortCalled) {
    ::testing::internal::CaptureStderr();
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    MockAubCenterFixture::setMockAubCenter(*rootDeviceEnvironment, CommandStreamReceiverType::aub, true);
    auto testAubCenter = std::make_unique<TestAubCenter>(*rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub);
    testAubCenter->resetAubManager();
    rootDeviceEnvironment->aubCenter = std::move(testAubCenter);

    rootDeviceEnvironment->useMockAubCenter = false;
    EXPECT_THROW(rootDeviceEnvironment->initAubCenter(true, "test.aub", CommandStreamReceiverType::aub), std::runtime_error);

    std::string output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("ERROR: Simulation mode detected but Aubstream is not available.\n"), std::string::npos);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsReceivesCorrectInputParams) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->initAubCenter(true, "test.aub", CommandStreamReceiverType::aub);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_TRUE(rootDeviceEnvironment->localMemoryEnabledReceived);
    EXPECT_STREQ(rootDeviceEnvironment->aubFileNameReceived.c_str(), "test.aub");
}

TEST(RootDeviceEnvironment, whenCreatingRootDeviceEnvironmentThenCreateOsAgnosticOsTime) {
    DebugManagerStateRestore dbgRestore;
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto profilingTimerResolution = CommonConstants::defaultProfilingTimerResolution;

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    EXPECT_EQ(nullptr, rootDeviceEnvironment->osTime.get());
    rootDeviceEnvironment->initOsTime();

    uint64_t ts = 123;
    EXPECT_TRUE(rootDeviceEnvironment->osTime->getCpuTime(&ts));
    EXPECT_EQ(0u, ts);

    EXPECT_EQ(0u, rootDeviceEnvironment->osTime->getHostTimerResolution());
    EXPECT_EQ(0u, rootDeviceEnvironment->osTime->getCpuRawTimestamp());

    TimeStampData tsData{1, 2};
    TimeQueryStatus error = rootDeviceEnvironment->osTime->getGpuCpuTime(&tsData);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(0u, tsData.gpuTimeStamp);

    EXPECT_EQ(profilingTimerResolution, rootDeviceEnvironment->osTime->getDynamicDeviceTimerResolution());
    EXPECT_EQ(static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution()), rootDeviceEnvironment->osTime->getDynamicDeviceTimerClock());

    struct MockOSTime : public OSTime {
        using OSTime::deviceTime;
    };
    auto deviceTime = static_cast<MockOSTime *>(rootDeviceEnvironment->osTime.get())->deviceTime.get();
    EXPECT_TRUE(deviceTime->isTimestampsRefreshEnabled());
    debugManager.flags.EnableReusingGpuTimestamps.set(0);
    EXPECT_FALSE(deviceTime->isTimestampsRefreshEnabled());
}

TEST(RootDeviceEnvironment, givenUseAubStreamFalseWhenGetAubManagerIsCalledThenReturnNull) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseAubStream.set(false);

    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), false, 1u};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::aub);
    auto aubManager = rootDeviceEnvironment->aubCenter->getAubManager();
    EXPECT_EQ(nullptr, aubManager);
}
TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), false, 1u};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::aub);
    auto currentAubCenter = rootDeviceEnvironment->aubCenter.get();
    EXPECT_NE(nullptr, currentAubCenter);
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::aub);
    EXPECT_EQ(currentAubCenter, rootDeviceEnvironment->aubCenter.get());
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
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->setRcsExposure();
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();

    EXPECT_TRUE(hwInfo->featureTable.flags.ftrRcsNode);
}

TEST(RootDeviceEnvironment, givenHardwareInfoAndDebugVariableNodeOrdinalEqualsCccsWhenPrepareDeviceEnvironmentsThenFtrRcsNodeIsTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->setRcsExposure();
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();

    EXPECT_TRUE(hwInfo->featureTable.flags.ftrRcsNode);
}

TEST(RootDeviceEnvironment, givenEnableAILFlagSetToFalseWhenInitializingAILConfigurationThenSkipInitializingIt) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableAIL.set(false);

    MockExecutionEnvironment executionEnvironment;
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    ASSERT_EQ(nullptr, rootDeviceEnvironment->ailConfiguration);
    rootDeviceEnvironment->initAilConfiguration();
    EXPECT_EQ(nullptr, rootDeviceEnvironment->ailConfiguration);
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
    debugManager.flags.EnableDirectSubmissionController.set(1);

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};
    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_NE(controller, nullptr);
}

TEST(ExecutionEnvironment, givenSetCsrFlagSetWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(1);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetZeroWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(0);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenExperimentalUSMAllocationReuseCleanerSetWhenInitializeUnifiedMemoryReuseCleanerThenNotNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalUSMAllocationReuseCleaner.set(1);

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.initializeUnifiedMemoryReuseCleaner(true);
    auto cleaner = executionEnvironment.unifiedMemoryReuseCleaner.get();

    EXPECT_NE(cleaner, nullptr);
    executionEnvironment.initializeUnifiedMemoryReuseCleaner(true);
    EXPECT_EQ(cleaner, executionEnvironment.unifiedMemoryReuseCleaner.get());
}

TEST(ExecutionEnvironment, givenExperimentalUSMAllocationReuseCleanerSetZeroWhenInitializeUnifiedMemoryReuseCleanerThenNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalUSMAllocationReuseCleaner.set(0);

    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.initializeUnifiedMemoryReuseCleaner(true);

    EXPECT_EQ(nullptr, executionEnvironment.unifiedMemoryReuseCleaner.get());
}

TEST(ExecutionEnvironment, givenExperimentalUSMAllocationReuseCleanerSetAndNotEnabledWhenInitializeUnifiedMemoryReuseCleanerThenForceInit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalUSMAllocationReuseCleaner.set(1);

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.initializeUnifiedMemoryReuseCleaner(false);
    auto cleaner = executionEnvironment.unifiedMemoryReuseCleaner.get();

    EXPECT_NE(cleaner, nullptr);
}

TEST(ExecutionEnvironment, givenNeoCalEnabledWhenCreateExecutionEnvironmentThenSetDebugVariables) {
    const std::unordered_map<std::string, int32_t> config = {
        {"UseKmdMigration", 0},
        {"SplitBcsSize", 256}};

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    EXPECT_EQ(defaultValue, debugManager.flags.variableName.getRef());
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    DebugManagerStateRestore restorer;
    debugManager.flags.NEO_CAL_ENABLED.set(1);
    ExecutionEnvironment exeEnv;

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)      \
    {                                                                                  \
        if constexpr (std::is_same_v<bool, dataType>) {                                \
            if (strcmp(#variableName, "NEO_CAL_ENABLED") == 0) {                       \
                EXPECT_TRUE(debugManager.flags.variableName.getRef());                 \
            } else {                                                                   \
                EXPECT_EQ(defaultValue, debugManager.flags.variableName.getRef());     \
            }                                                                          \
        } else {                                                                       \
            if constexpr (std::is_same_v<int32_t, dataType>) {                         \
                auto it = config.find(#variableName);                                  \
                if (it != config.end()) {                                              \
                    EXPECT_EQ(it->second, debugManager.flags.variableName.getRef());   \
                } else {                                                               \
                    EXPECT_EQ(defaultValue, debugManager.flags.variableName.getRef()); \
                }                                                                      \
            } else {                                                                   \
                EXPECT_EQ(defaultValue, debugManager.flags.variableName.getRef());     \
            }                                                                          \
        }                                                                              \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
}

TEST(ExecutionEnvironment, givenEnvVarUsedInCalConfigAlsoSetByAppWhenCreateExecutionEnvironmentThenRespectAppSetting) {
    constexpr int32_t appCommandBufferAlignment = 12345;
    DebugManagerStateRestore restorer;
    debugManager.flags.NEO_CAL_ENABLED.set(1);
    debugManager.flags.ForceCommandBufferAlignment.set(appCommandBufferAlignment);
    ExecutionEnvironment exeEnv;

    EXPECT_EQ(debugManager.flags.ForceCommandBufferAlignment.get(), appCommandBufferAlignment);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.initializeMemoryManager();
    ASSERT_NE(nullptr, executionEnvironment.memoryManager);
    EXPECT_TRUE(executionEnvironment.memoryManager->isInitialized());
    EXPECT_NE(0u, executionEnvironment.memoryManager->usmReuseInfo.getMaxAllocationsSavedForReuseSize());
}

static_assert(sizeof(ExecutionEnvironment) == sizeof(std::unique_ptr<MemoryManager>) +
                                                  sizeof(std::unique_ptr<DirectSubmissionController>) +
                                                  sizeof(std::unique_ptr<UnifiedMemoryReuseCleaner>) +
                                                  sizeof(std::unique_ptr<OsEnvironment>) +
                                                  sizeof(std::vector<std::unique_ptr<RootDeviceEnvironment>>) +
                                                  sizeof(std::unordered_map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t>>) +
                                                  sizeof(std::unordered_map<std::thread::id, std::string>) +
                                                  2 * sizeof(std::mutex) +
                                                  2 * sizeof(bool) +
                                                  sizeof(DeviceHierarchyMode) +
                                                  sizeof(DebuggingMode) +
                                                  sizeof(std::unordered_map<uint32_t, uint32_t>) +
                                                  sizeof(std::mutex) +
                                                  sizeof(std::vector<std::tuple<std::string, uint32_t>>) +
                                                  (is64bit ? 22 : 14),
              "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 8> {
        MemoryMangerMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {
            callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
            callBasePopulateOsHandles = false;
        }
    };
    struct UnifiedMemoryReuseCleanerMock : public DestructorCounted<UnifiedMemoryReuseCleaner, 7> {
        UnifiedMemoryReuseCleanerMock(uint32_t &destructorId) : DestructorCounted(destructorId, false) {}
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
        AubCenterMock(uint32_t &destructorId, const RootDeviceEnvironment &rootDeviceEnvironment) : DestructorCounted(destructorId, rootDeviceEnvironment, false, "", CommandStreamReceiverType::aub) {}
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
    executionEnvironment->unifiedMemoryReuseCleaner = std::make_unique<UnifiedMemoryReuseCleanerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(9u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleRootDevicesWhenTheyAreCreatedThenReuseMemoryManager) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.incRefInternal();
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment.calculateMaxOsContextCount();

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
    osInterface->setDriverModel(std::make_unique<DefaultDriverModelMock>(DriverModelType::unknown));

    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::move(osInterface);

    executionEnvironment.prepareForCleanup();

    EXPECT_EQ(1u, isDriverAvailableCounter);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<DefaultDriverModelMock>(DriverModelType::unknown));
}

TEST(ExecutionEnvironment, givenUnproperSetCsrFlagValueWhenInitializingMemoryManagerThenCreateDefaultMemoryManager) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(10);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, whenCalculateMaxOsContexCountThenGlobalVariableHasProperValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(0);
    VariableBackup<uint32_t> osContextCountBackup(&MemoryManager::maxOsContextCount, 0);
    uint32_t numRootDevices = 17u;
    uint32_t expectedOsContextCount = 0u;
    uint32_t expectedOsContextCountForCcs = 0u;

    {
        MockExecutionEnvironment executionEnvironment(nullptr, true, numRootDevices);

        for (const auto &rootDeviceEnvironment : executionEnvironment.rootDeviceEnvironments) {
            auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
            auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
            auto osContextCount = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment).size();
            auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
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

TEST(ExecutionEnvironment, givenCorrectZeAffinityMaskWithFlatOrCombinedHierarchyThenMapOfSubDeviceIndicesIsSet) {
    DebugManagerStateRestore restore;

    debugManager.flags.CreateMultipleSubDevices.set(4);
    debugManager.flags.ZE_AFFINITY_MASK.set("3");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    DeviceHierarchyMode deviceHierarchyModes[] = {DeviceHierarchyMode::flat, DeviceHierarchyMode::combined};
    for (auto deviceHierarchyMode : deviceHierarchyModes) {
        MockExecutionEnvironment executionEnvironment(&hwInfo);
        executionEnvironment.incRefInternal();
        executionEnvironment.setDeviceHierarchyMode(deviceHierarchyMode);

        DeviceFactory::createDevices(executionEnvironment);

        EXPECT_FALSE(executionEnvironment.mapOfSubDeviceIndices.empty());
    }
}

TEST(ExecutionEnvironment, givenIncorrectZeAffinityMaskWithFlatOrCombinedHierarchyThenMapOfSubDeviceIndicesIsEmpty) {
    DebugManagerStateRestore restore;

    debugManager.flags.CreateMultipleSubDevices.set(4);
    debugManager.flags.ZE_AFFINITY_MASK.set("4");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    DeviceHierarchyMode deviceHierarchyModes[] = {DeviceHierarchyMode::flat, DeviceHierarchyMode::combined};
    for (auto deviceHierarchyMode : deviceHierarchyModes) {
        MockExecutionEnvironment executionEnvironment(&hwInfo);
        executionEnvironment.incRefInternal();
        executionEnvironment.setDeviceHierarchyMode(deviceHierarchyMode);

        DeviceFactory::createDevices(executionEnvironment);

        EXPECT_TRUE(executionEnvironment.mapOfSubDeviceIndices.empty());
    }
}

TEST(ExecutionEnvironment, givenBuiltinsSetWhenRootDeviceEnvironmentIsReleasedThenBuiltinsIsReset) {
    auto hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->builtins.reset(new BuiltIns);
    executionEnvironment.releaseRootDeviceEnvironmentResources(executionEnvironment.rootDeviceEnvironments[0].get());
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->builtins);
}

TEST(ExecutionEnvironmentWithAILTests, whenAILConfigurationIsNullptrAndEnableAILFlagIsTrueWhenInitializingAILThenReturnFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableAIL.set(true);

    MockExecutionEnvironment executionEnvironment{};
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->ailInitializationResult = {};
    rootDeviceEnvironment->ailConfiguration.reset(nullptr);

    EXPECT_FALSE(rootDeviceEnvironment->initAilConfiguration());
}

TEST(ExecutionEnvironmentWithAILTests, whenPlatformHasNoAILHelperAvailableAndEnableAILFlagIsFalseWhenInitializingAILThenReturnTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableAIL.set(false);

    MockExecutionEnvironment executionEnvironment{};
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->ailInitializationResult = {};
    rootDeviceEnvironment->ailConfiguration.reset(nullptr);

    EXPECT_TRUE(rootDeviceEnvironment->initAilConfiguration());
}

TEST(ExecutionEnvironmentWithAILTests, whenAILConfigurationFailsOnInitProcessExecutableNameThenAILInitializationReturnFalse) {
    MockExecutionEnvironment executionEnvironment{};
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->ailInitializationResult = {};

    rootDeviceEnvironment->ailConfiguration.reset(new MockAILConfiguration());
    auto mockAILConfiguration = static_cast<MockAILConfiguration *>(rootDeviceEnvironment->ailConfiguration.get());
    mockAILConfiguration->initProcessExecutableNameResult = false;
    ASSERT_NE(nullptr, rootDeviceEnvironment->ailConfiguration);

    EXPECT_FALSE(rootDeviceEnvironment->initAilConfiguration());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenSetErrorDescriptionIsCalledThenGetErrorDescriptionGetsStringCorrectly) {
    std::string errorString = "we manually created error";
    std::string errorString2 = "here's the next string to pass with arguments: ";
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setErrorDescription(std::string(errorString));

    const char *pStr = nullptr;
    executionEnvironment.getErrorDescription(&pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));

    int result = [](MockExecutionEnvironment *exeEnv, const std::string &str) {
        int res = exeEnv->setErrorDescription(std::string(str));
        return res;
    }(&executionEnvironment, errorString2);
    EXPECT_NE(0, result);
    executionEnvironment.getErrorDescription(&pStr);
    std::string expectedString = errorString2;
    EXPECT_EQ(0, strcmp(expectedString.c_str(), pStr));
}

TEST(ExecutionEnvironment, givenValidExecutionEnvironmentWhenClearErrorDescriptionIsCalledThenEmptyStringIsReturned) {
    std::string errorString = "error string";
    std::string emptyString = "";
    const char *pStr = nullptr;
    MockExecutionEnvironment executionEnvironment;

    int result = executionEnvironment.clearErrorDescription();
    EXPECT_EQ(0, result);
    executionEnvironment.getErrorDescription(&pStr);
    EXPECT_EQ(0, strcmp(emptyString.c_str(), pStr));
    executionEnvironment.setErrorDescription(errorString);
    executionEnvironment.getErrorDescription(&pStr);

    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));
    EXPECT_EQ(0, result);

    result = executionEnvironment.clearErrorDescription();
    EXPECT_EQ(0, result);
    executionEnvironment.getErrorDescription(&pStr);
    EXPECT_EQ(0, strcmp(emptyString.c_str(), pStr));
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenGetErrorDescriptionIsCalledThenEmptyStringIsReturned) {
    std::string errorString = "";
    MockExecutionEnvironment executionEnvironment;

    const char *pStr = nullptr;
    executionEnvironment.getErrorDescription(&pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));

    executionEnvironment.setErrorDescription(errorString);
    executionEnvironment.getErrorDescription(&pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));
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
    debugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

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
    debugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(0);
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

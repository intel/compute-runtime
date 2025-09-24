/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_interface.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
extern ApiSpecificConfig::ApiType apiTypeForUlts;
namespace NEO {
extern bool isDeviceUsmPoolingEnabledForUlts;
}

TEST(DeviceBlitterTest, whenBlitterOperationsSupportIsDisabledThenNoInternalCopyEngineIsReturned) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = false;

    UltDeviceFactory factory{1, 0};
    EXPECT_EQ(nullptr, factory.rootDevices[0]->getInternalCopyEngine());
}

TEST(DeviceBlitterTest, givenForceBCSForInternalCopyEngineToIndexZeroWhenGetInternalCopyEngineIsCalledThenInternalMainCopyEngineIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceBCSForInternalCopyEngine.set(0);

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;

    UltDeviceFactory factory{1, 0};
    factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::internal});
    auto engine = factory.rootDevices[0]->getInternalCopyEngine();
    EXPECT_NE(nullptr, engine);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, engine->getEngineType());
    EXPECT_EQ(EngineUsage::internal, engine->getEngineUsage());
}

TEST(DeviceBlitterTest, givenForceBCSForInternalCopyEngineToIndexOneWhenGetInternalLinkCopyEngineIsCalledThenInternalLinkCopyEngineOneIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceBCSForInternalCopyEngine.set(1);

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;

    UltDeviceFactory factory{1, 0};
    factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS1, EngineUsage::internal});
    auto engine = factory.rootDevices[0]->getInternalCopyEngine();
    EXPECT_NE(nullptr, engine);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, engine->getEngineType());
    EXPECT_EQ(EngineUsage::internal, engine->getEngineUsage());
}

TEST(DeviceBlitterTest, givenBlitterOperationsDisabledWhenCreatingBlitterEngineThenAbort) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = false;

    UltDeviceFactory factory{1, 0};
    EXPECT_THROW(factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::cooperative}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::internal}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::lowPriority}), std::runtime_error);
}

TEST(Device, givenNoDebuggerWhenGettingDebuggerThenNullptrIsReturned) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(nullptr, device->getDebugger());
    EXPECT_EQ(nullptr, device->getL0Debugger());
}

struct DeviceWithDisabledL0DebuggerTests : public DeviceFixture, public ::testing::Test {
    void SetUp() override {
        debugManager.flags.DisableSupportForL0Debugger.set(true);
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore dbgRestorer;
};

TEST_F(DeviceWithDisabledL0DebuggerTests, givenSetFlagDisableSupportForL0DebuggerWhenCreateDeviceThenCapabilityL0DebuggerSupportedIsDisabled) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.l0DebuggerSupported);
}

TEST(Device, givenDeviceWithBrandingStringNameWhenGettingDeviceNameThenBrandingStringIsReturned) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.deviceName = "Custom Device";
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    EXPECT_STREQ("Custom Device", device->getDeviceName().c_str());
}

TEST(Device, givenDeviceWithoutBrandingStringNameWhenGettingDeviceNameThenGenericNameWithHexadecimalDeviceIdIsReturned) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.deviceName = "";
    hwInfo.platform.usDeviceID = 0x1AB;
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    EXPECT_STREQ("Intel(R) Graphics [0x01ab]", device->getDeviceName().c_str());
}

TEST(Device, WhenCreatingDeviceThenCapsInitilizedBeforeEnginesAreCreated) {

    class CapsInitMockDevice : public RootDevice {
      public:
        using Device::createDeviceImpl;
        using RootDevice::RootDevice;

        void initializeCaps() override {
            capsInitialized = true;
            return Device::initializeCaps();
        }

        bool createEngines() override {
            capsInitializedWhenCreatingEngines = capsInitialized;
            return RootDevice::createEngines();
        }

        bool capsInitialized = false;
        bool capsInitializedWhenCreatingEngines = false;
    };

    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.deviceName = "";
    hwInfo.platform.usDeviceID = 0x1AB;

    auto executionEnvironment = new ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->setDeviceHierarchyMode(executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->initializeMemoryManager();

    auto device = std::make_unique<CapsInitMockDevice>(executionEnvironment, 0);
    device->createDeviceImpl();

    EXPECT_TRUE(device->capsInitializedWhenCreatingEngines);
}

using DeviceTest = Test<DeviceFixture>;

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledAndRtBackedBufferIsNullptrThenMemoryBackedBufferIsCreated) {
    if (pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    EXPECT_EQ(nullptr, pDevice->getRTMemoryBackedBuffer());
    EXPECT_EQ(false, pDevice->rayTracingIsInitialized());
    pDevice->initializeRayTracing(0);
    EXPECT_NE(nullptr, pDevice->getRTMemoryBackedBuffer());
    EXPECT_EQ(true, pDevice->rayTracingIsInitialized());
    pDevice->initializeRayTracing(0);
    EXPECT_NE(nullptr, pDevice->getRTMemoryBackedBuffer());
    EXPECT_EQ(true, pDevice->rayTracingIsInitialized());
}

TEST_F(DeviceTest, whenGetRTDispatchGlobalsIsCalledWithUnsupportedBVHLevelsThenNullptrIsReturned) {
    pDevice->initializeRayTracing(5);
    EXPECT_EQ(nullptr, pDevice->getRTDispatchGlobals(100));
}

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledWithMockAllocatorThenDispatchGlobalsArrayAllocationIsLockable) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    auto maxBvhLevel = 3;
    pDevice->initializeRayTracing(maxBvhLevel);
    for (auto i = 0; i < maxBvhLevel; i++) {
        auto rtDispatchGlobals = pDevice->getRTDispatchGlobals(i);
        EXPECT_NE(nullptr, rtDispatchGlobals);
        auto dispatchGlobalsArray = rtDispatchGlobals->rtDispatchGlobalsArray;
        EXPECT_NE(nullptr, dispatchGlobalsArray);
        EXPECT_FALSE(dispatchGlobalsArray->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);
    }
}

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledWithMockAllocatorThenRTDispatchGlobalsIsAllocated) {
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(5));
}

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledMultipleTimesWithMockAllocatorThenInitializeRayTracingIsIdempotent) {
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(5));
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(5));
}

TEST_F(DeviceTest, whenGetRTDispatchGlobalsIsCalledBeforeInitializationThenNullPtrIsReturned) {
    EXPECT_EQ(nullptr, pDevice->getRTDispatchGlobals(1));
}

TEST_F(DeviceTest, whenGetRTDispatchGlobalsIsCalledWithZeroSizeAndMockAllocatorThenDispatchGlobalsIsReturned) {
    EXPECT_EQ(nullptr, pDevice->getRTDispatchGlobals(0));
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(0));
}

TEST_F(DeviceTest, whenAllocateRTDispatchGlobalsIsCalledThenRTDispatchGlobalsIsAllocated) {
    pDevice->initializeRayTracing(5);
    pDevice->allocateRTDispatchGlobals(3);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
}

TEST_F(DeviceTest, whenAllocateRTDispatchGlobalsIsCalledThenStackSizePerRayIsSetCorrectly) {
    pDevice->initializeRayTracing(5);
    pDevice->allocateRTDispatchGlobals(3);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
    RTDispatchGlobals dispatchGlobals = *reinterpret_cast<struct RTDispatchGlobals *>(pDevice->getRTDispatchGlobals(3)->rtDispatchGlobalsArray->getUnderlyingBuffer());

    auto releaseHelper = getReleaseHelper();
    if (releaseHelper) {
        EXPECT_EQ(dispatchGlobals.stackSizePerRay, releaseHelper->getStackSizePerRay());
    } else {
        EXPECT_EQ(dispatchGlobals.stackSizePerRay, 0u);
    }
}

TEST_F(DeviceTest, givenNot48bResourceForRtWhenAllocateRTDispatchGlobalsIsCalledThenRTDispatchGlobalsIsAllocatedWithout48bResourceFlag) {
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = false;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironmentRef();
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->validateAllocateProperties = [](const AllocationProperties &properties) -> void {
        EXPECT_FALSE(properties.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    pDevice->initializeRayTracing(5);
    pDevice->allocateRTDispatchGlobals(3);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

HWTEST2_F(DeviceTest, whenAllocateRTDispatchGlobalsIsCalledAndRTStackAllocationFailsRTDispatchGlobalsIsNotAllocated, IsPVC) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    std::unique_ptr<NEO::MemoryManager> otherMemoryManager;
    otherMemoryManager = std::make_unique<NEO::MockMemoryManagerWithCapacity>(*device->executionEnvironment);
    static_cast<NEO::MockMemoryManagerWithCapacity &>(*otherMemoryManager).capacity = MemoryConstants::pageSize;
    device->executionEnvironment->memoryManager.swap(otherMemoryManager);

    device->initializeRayTracing(5);
    EXPECT_EQ(nullptr, device->getRTDispatchGlobals(3));

    device->executionEnvironment->memoryManager.swap(otherMemoryManager);
}

TEST_F(DeviceTest, givenDispatchGlobalsAllocationFailsThenRTDispatchGlobalsInfoIsNull) {
    std::unique_ptr<NEO::MemoryManager> otherMemoryManager;
    otherMemoryManager = std::make_unique<NEO::FailMemoryManager>(1, *pDevice->getExecutionEnvironment());
    pDevice->getExecutionEnvironment()->memoryManager.swap(otherMemoryManager);

    pDevice->initializeRayTracing(5);
    auto rtDispatchGlobalsInfo = pDevice->getRTDispatchGlobals(5);

    EXPECT_EQ(nullptr, rtDispatchGlobalsInfo);

    pDevice->getExecutionEnvironment()->memoryManager.swap(otherMemoryManager);
}

TEST_F(DeviceTest, GivenDeviceWhenGenerateUuidThenValidValuesAreSet) {
    std::array<uint8_t, ProductHelper::uuidSize> uuid, expectedUuid;
    pDevice->generateUuid(uuid);
    uint32_t rootDeviceIndex = pDevice->getRootDeviceIndex();

    expectedUuid.fill(0);
    uint16_t vendorId = 0x8086; // Intel
    uint16_t deviceId = static_cast<uint16_t>(pDevice->getHardwareInfo().platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(pDevice->getHardwareInfo().platform.usRevId);

    memcpy_s(&expectedUuid[0], sizeof(uint16_t), &vendorId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[2], sizeof(uint16_t), &deviceId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[4], sizeof(uint16_t), &revisionId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[6], sizeof(uint32_t), &rootDeviceIndex, sizeof(rootDeviceIndex));

    EXPECT_EQ(memcmp(&uuid, &expectedUuid, ProductHelper::uuidSize), 0);
}

TEST_F(DeviceTest, GivenDeviceWhenGenerateUuidFromPciBusInfoThenValidValuesAreSet) {
    std::array<uint8_t, ProductHelper::uuidSize> uuid, expectedUuid;

    PhysicalDevicePciBusInfo pciBusInfo = {1, 1, 1, 1};

    pDevice->generateUuidFromPciBusInfo(pciBusInfo, uuid);

    expectedUuid.fill(0);
    uint16_t vendorId = 0x8086; // Intel
    uint16_t deviceId = static_cast<uint16_t>(pDevice->getHardwareInfo().platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(pDevice->getHardwareInfo().platform.usRevId);
    uint16_t pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    uint8_t pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
    uint8_t pciDevice = static_cast<uint8_t>(pciBusInfo.pciDevice);
    uint8_t pciFunction = static_cast<uint8_t>(pciBusInfo.pciFunction);

    memcpy_s(&expectedUuid[0], sizeof(uint16_t), &vendorId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[2], sizeof(uint16_t), &deviceId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[4], sizeof(uint16_t), &revisionId, sizeof(uint16_t));
    memcpy_s(&expectedUuid[6], sizeof(uint16_t), &pciDomain, sizeof(uint16_t));
    memcpy_s(&expectedUuid[8], sizeof(uint8_t), &pciBus, sizeof(uint8_t));
    memcpy_s(&expectedUuid[9], sizeof(uint8_t), &pciDevice, sizeof(uint8_t));
    memcpy_s(&expectedUuid[10], sizeof(uint8_t), &pciFunction, sizeof(uint8_t));

    EXPECT_EQ(memcmp(&uuid, &expectedUuid, ProductHelper::uuidSize), 0);
}

TEST_F(DeviceTest, givenDeviceWhenUsingBufferPoolsTrackingThenCountIsUpdated) {
    pDevice->updateMaxPoolCount(3u);
    EXPECT_EQ(3u, pDevice->maxBufferPoolCount);
    EXPECT_EQ(0u, pDevice->bufferPoolCount.load());

    EXPECT_FALSE(pDevice->requestPoolCreate(4u));
    EXPECT_EQ(0u, pDevice->bufferPoolCount.load());

    EXPECT_TRUE(pDevice->requestPoolCreate(3u));
    EXPECT_EQ(3u, pDevice->bufferPoolCount.load());

    EXPECT_FALSE(pDevice->requestPoolCreate(1u));
    EXPECT_EQ(3u, pDevice->bufferPoolCount.load());

    pDevice->recordPoolsFreed(2u);
    EXPECT_EQ(1u, pDevice->bufferPoolCount.load());
}

using DeviceGetCapsTest = Test<DeviceFixture>;

TEST_F(DeviceGetCapsTest, givenMockCompilerInterfaceWhenInitializeCapsIsCalledThenMaxParameterSizeIsSetCorrectly) {
    auto pCompilerInterface = new MockCompilerInterface;
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);
    pDevice->maxParameterSizeFromIGC = 2u;
    pDevice->callBaseGetMaxParameterSizeFromIGC = true;
    MockIgcFeaturesAndWorkarounds mockIgcFtrWa;
    pCompilerInterface->igcFeaturesAndWorkaroundsTagOCL = &mockIgcFtrWa;

    mockIgcFtrWa.maxOCLParamSize = 0u;
    pDevice->initializeCaps();
    EXPECT_EQ(2048u, pDevice->getDeviceInfo().maxParameterSize);

    mockIgcFtrWa.maxOCLParamSize = 1u;
    pDevice->initializeCaps();
    EXPECT_EQ(1u, pDevice->getDeviceInfo().maxParameterSize);
}

TEST_F(DeviceGetCapsTest,
       givenImplicitScalingWhenInitializeCapsIsCalledThenMaxMemAllocSizeIsSetCorrectly) {
    DebugManagerStateRestore dbgRestorer;

    debugManager.flags.CreateMultipleSubDevices.set(4);
    pDevice->deviceBitfield = 15;

    debugManager.flags.EnableWalkerPartition.set(1);
    pDevice->initializeCaps();
    EXPECT_EQ(pDevice->getDeviceInfo().maxMemAllocSize, pDevice->getDeviceInfo().globalMemSize);

    debugManager.flags.EnableWalkerPartition.set(0);
    pDevice->initializeCaps();
    EXPECT_LE(pDevice->getDeviceInfo().maxMemAllocSize, pDevice->getDeviceInfo().globalMemSize);
}

TEST_F(DeviceGetCapsTest,
       givenImplicitScalingTrueWhenInitializeCapsIsCalledThenMaxMemAllocSizeIsSetCorrectly) {
    DebugManagerStateRestore dbgRestorer;

    debugManager.flags.CreateMultipleSubDevices.set(4);
    pDevice->deviceBitfield = 15;

    debugManager.flags.EnableImplicitScaling.set(1);
    debugManager.flags.EnableWalkerPartition.set(1);
    pDevice->initializeCaps();
    EXPECT_EQ(pDevice->getDeviceInfo().maxMemAllocSize, pDevice->getDeviceInfo().globalMemSize);
}

TEST_F(DeviceGetCapsTest,
       givenImplicitScalingFalseWhenInitializeCapsIsCalledThenMaxMemAllocSizeIsSetCorrectly) {
    DebugManagerStateRestore dbgRestorer;

    debugManager.flags.CreateMultipleSubDevices.set(4);
    pDevice->deviceBitfield = 15;

    debugManager.flags.EnableImplicitScaling.set(0);
    debugManager.flags.EnableWalkerPartition.set(1);
    pDevice->initializeCaps();
    EXPECT_LE(pDevice->getDeviceInfo().maxMemAllocSize, pDevice->getDeviceInfo().globalMemSize);
}

TEST_F(DeviceGetCapsTest, givenDontForcePreemptionModeDebugVariableWhenCreateDeviceThenSetDefaultHwPreemptionMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        debugManager.flags.ForcePreemptionMode.set(-1);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        EXPECT_TRUE(device->getHardwareInfo().capabilityTable.defaultPreemptionMode ==
                    device->getPreemptionMode());
    }
}

TEST_F(DeviceGetCapsTest, givenDebugFlagSetWhenCreatingDeviceInfoThenOverrideProfilingTimerResolution) {
    DebugManagerStateRestore dbgRestorer;

    debugManager.flags.OverrideProfilingTimerResolution.set(123);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(double(123), device->getDeviceInfo().profilingTimerResolution);
    EXPECT_EQ(123u, device->getDeviceInfo().outProfilingTimerResolution);
}

TEST_F(DeviceGetCapsTest, givenForcePreemptionModeDebugVariableWhenCreateDeviceThenSetForcedMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        PreemptionMode forceMode = PreemptionMode::MidThread;
        if (defaultHwInfo->capabilityTable.defaultPreemptionMode == forceMode) {
            // force non-default mode
            forceMode = PreemptionMode::ThreadGroup;
        }
        debugManager.flags.ForcePreemptionMode.set((int32_t)forceMode);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

        EXPECT_TRUE(forceMode == device->getPreemptionMode());
    }
}

TEST_F(DeviceGetCapsTest, givenDeviceWithMidThreadPreemptionWhenDeviceIsCreatedThenSipKernelIsNotCreated) {
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    DebugManagerStateRestore dbgRestorer;
    {
        auto builtIns = new MockBuiltins();
        MockSipData::called = false;

        debugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::MidThread);

        auto executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0u].get(), builtIns);
        auto device = std::unique_ptr<Device>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
        ASSERT_EQ(builtIns, device->getBuiltIns());
        EXPECT_FALSE(MockSipData::called);
    }
}

TEST_F(DeviceGetCapsTest, whenDriverModelHasLimitationForMaxMemoryAllocationSizeThenTakeItIntoAccount) {
    DebugManagerStateRestore dbgRestorer;
    size_t maxAllocSizeTestValue = 512;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    auto driverModel = std::make_unique<MockDriverModel>();
    driverModel->maxAllocSize = maxAllocSizeTestValue;
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(driverModel));
    device->initializeCaps();
    const auto &caps = device->getDeviceInfo();
    EXPECT_EQ(maxAllocSizeTestValue, caps.maxMemAllocSize);
}

TEST(DeviceGetCapsSimpleTest, givenVariousOclVersionsWhenCapsAreCreatedThenDeviceReportsSpirvAsSupportedIl) {
    DebugManagerStateRestore dbgRestorer;
    int32_t oclVersionsToTest[] = {12, 21, 30};
    for (auto oclVersion : oclVersionsToTest) {
        debugManager.flags.ForceOCLVersion.set(oclVersion);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("SPIR-V_1.5 SPIR-V_1.4 SPIR-V_1.3 SPIR-V_1.2 SPIR-V_1.1 SPIR-V_1.0 ", caps.ilVersion);
    }
}

TEST(DeviceGetCapsSimpleTest, givenDebugFlagToSetWorkgroupSizeWhenDeviceIsCreatedThenItUsesThatWorkgroupSize) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.OverrideMaxWorkgroupSize.set(16u);

    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(16u, device->getDeviceInfo().maxWorkGroupSize);
}

TEST_F(DeviceGetCapsTest, givenFlagEnabled64kbPagesWhenCallConstructorMemoryManagerThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    VariableBackup<bool> osEnabled64kbPagesBackup(&OSInterface::osEnabled64kbPages);
    class MockMemoryManager : public MemoryManager {
      public:
        MockMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {}
        void addAllocationToHostPtrManager(GraphicsAllocation *memory) override{};
        void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override{};
        GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; };
        AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
        void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
        void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override{};
        void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
        uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override {
            return 0;
        };
        uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
        double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
        AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override {
            return {};
        }
        AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
            return {};
        }
        size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override {
            *heap = HeapIndex::heapStandard;
            return MemoryConstants::pageSize64k;
        }
        void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
        AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override { return {}; }
        void freeCpuAddress(AddressRange addressRange) override{};
        GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
        GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
        GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
        bool unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override { return false; };
        bool unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
        bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
        bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };

        GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
        GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override { return nullptr; };
        void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override { return nullptr; };
        void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override{};
    };

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    std::unique_ptr<MemoryManager> memoryManager;

    debugManager.flags.Enable64kbpages.set(-1);

    OSInterface::osEnabled64kbPages = false;
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    OSInterface::osEnabled64kbPages = true;
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));

    debugManager.flags.Enable64kbpages.set(0); // force false
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    debugManager.flags.Enable64kbpages.set(1); // force true
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));
}

using DeviceTests = ::testing::Test;

TEST_F(DeviceTests, givenDispatchGlobalsAllocationFailsOnSecondSubDeviceThenRtDispatchGlobalsInfoIsNull) {
    class FailMockMemoryManager : public MockMemoryManager {
      public:
        FailMockMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(false, false, executionEnvironment) {}

        GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
            allocateGraphicsMemoryWithPropertiesCount++;
            if (allocateGraphicsMemoryWithPropertiesCount > 2) {
                return nullptr;
            } else {
                return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties);
            }
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWalkerPartition.set(-1);
    debugManager.flags.CreateMultipleSubDevices.set(2u);

    UltDeviceFactory deviceFactory{1, 2};
    ExecutionEnvironment &executionEnvironment = *deviceFactory.rootDevices[0]->executionEnvironment;
    executionEnvironment.memoryManager = std::make_unique<FailMockMemoryManager>(executionEnvironment);

    deviceFactory.rootDevices[0]->initializeRayTracing(5);
    auto rtDispatchGlobalsInfo = deviceFactory.rootDevices[0]->getRTDispatchGlobals(5);

    EXPECT_EQ(nullptr, rtDispatchGlobalsInfo);
}

TEST_F(DeviceTests, givenMtPreemptionEnabledWhenCreatingRootCsrThenCreatePreemptionAllocation) {
    DebugManagerStateRestore restorer;

    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.ForcePreemptionMode.set(4);

    UltDeviceFactory deviceFactory{1, 2};

    EXPECT_TRUE(deviceFactory.rootDevices[0]->getDefaultEngine().osContext->isRootDevice());
    EXPECT_NE(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getPreemptionAllocation());
}

TEST_F(DeviceTests, givenPreemptionModeWhenOverridePreemptionModeThenProperlySet) {
    auto newPreemptionMode = PreemptionMode::ThreadGroup;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->overridePreemptionMode(newPreemptionMode);
    EXPECT_EQ(newPreemptionMode, device->getPreemptionMode());

    newPreemptionMode = PreemptionMode::Disabled;
    device->overridePreemptionMode(newPreemptionMode);
    EXPECT_EQ(newPreemptionMode, device->getPreemptionMode());
}

TEST_F(DeviceTests, givenNullAubcenterThenAdjustCcsCountDoesNotThrow) {
    auto hwInfo = *defaultHwInfo;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("4");

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1u);
    executionEnvironment.incRefInternal();
    EXPECT_NO_THROW(executionEnvironment.adjustCcsCount());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenZexNumberOfCssEnvVariableDefinedForNonPvcWhenDeviceIsCreatedThenCreateDevicesWithProperCcsCount) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("1");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();
    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount();
        EXPECT_EQ(std::min(1u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled), hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenDeviceIsCreatedWithEmptyZexNumberOfCssEnvVariableAndHwInfoCcsCountIsModifiedWhenAdjustCcsCountForSpecificDeviceIsInvokedThenVerifyCcsCountIsAdjustedToOne) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(1);
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto device = deviceFactory.rootDevices[0];

    auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
    auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
    EXPECT_EQ(1u, computeEngineGroup.engines.size());

    auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    executionEnvironment.adjustCcsCount();
    EXPECT_EQ(1u, hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZexNumberOfCssEnvVariableDefinedWhenDeviceIsCreatedThenCreateDevicesWithProperCcsCount) {

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:4,1:1,2:2,3:1");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 4);
    executionEnvironment.incRefInternal();
    UltDeviceFactory deviceFactory{4, 0, executionEnvironment};

    {
        auto device = deviceFactory.rootDevices[0];

        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        auto expectedNumberOfCcs = std::min(4u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
        EXPECT_EQ(expectedNumberOfCcs, computeEngineGroup.engines.size());
    }
    {
        auto device = deviceFactory.rootDevices[1];

        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        EXPECT_EQ(1u, computeEngineGroup.engines.size());
    }
    {
        auto device = deviceFactory.rootDevices[2];

        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        auto expectedNumberOfCcs = std::min(2u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
        EXPECT_EQ(expectedNumberOfCcs, computeEngineGroup.engines.size());
    }
    {
        auto device = deviceFactory.rootDevices[3];

        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        EXPECT_EQ(1u, computeEngineGroup.engines.size());
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenDeviceIsCreatedWithMalformedZexNumberOfCssEnvVariableDefinedAndHwInfoCcsCountIsSetToDefaultWhenAdjustCcsCountForSpecificRootDeviceIsInvokedThenVerifyHwInfoCcsCountIsSet) {

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};
    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount(0);
        EXPECT_EQ(1u, hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenDeviceIsCreatedWithZexNumberOfCssEnvVariableDefinedForXeHpAndHwInfoCcsCountIsSetToDefaultWhenAdjustCcsCountForSpecificRootDeviceIsInvokedThenVerifyHwInfoCcsCountIsRestoredForAllDevices) {

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 2);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{2, 0, executionEnvironment};
    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount(0);
        EXPECT_EQ(std::min(2u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled), hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }

    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount(1);
        EXPECT_EQ(std::min(2u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled), hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenDeviceIsCreatedWithZexNumberOfCssEnvVariableDefinedAndHwInfoCcsCountIsSetToDefaultWhenAdjustCcsCountForSpecificRootDeviceIsInvokedThenVerifyHwInfoCcsCountIsRestored) {

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:1,1:2");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 2);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{2, 0, executionEnvironment};
    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount(0);
        EXPECT_EQ(1u, hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }

    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount(1);
        EXPECT_EQ(std::min(2u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled), hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenDeviceIsCreatedWithAmbiguousZexNumberOfCssEnvVariableAndHwInfoCcsCountIsModifiedWhenAdjustCcsCountForSpecificDeviceIsInvokedThenVerifyCcsCountIsAdjustedToOne) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(1);

    for (const auto &numberOfCcsString : {"default", "", "0"}) {
        debugManager.flags.ZEX_NUMBER_OF_CCS.set(numberOfCcsString);

        auto hwInfo = *defaultHwInfo;

        MockExecutionEnvironment executionEnvironment(&hwInfo);
        executionEnvironment.incRefInternal();

        UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

        auto device = deviceFactory.rootDevices[0];

        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        EXPECT_EQ(1u, computeEngineGroup.engines.size());

        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount(0);
        EXPECT_EQ(1u, hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZexNumberOfCssAndZeAffinityMaskSetWhenDeviceIsCreatedThenProperNumberOfCcsIsExposed) {

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.CreateMultipleRootDevices.set(2);
    debugManager.flags.ZE_AFFINITY_MASK.set("1");
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:1,1:2");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 2);
    executionEnvironment.incRefInternal();

    auto devices = DeviceFactory::createDevices(executionEnvironment);

    {
        auto device = devices[0].get();

        EXPECT_EQ(0u, device->getRootDeviceIndex());
        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        EXPECT_EQ(1u, computeEngineGroup.engines.size());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenZeAffinityMaskSetAndTilesAsDevicesModelThenProperSubDeviceHierarchyMapisSet) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "FLAT"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    uint32_t expectedRootDevices = 4;
    debugManager.flags.ZE_AFFINITY_MASK.set("0,3,4,1.1,9,15,25");

    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);
    std::vector<uint32_t> expectedRootDeviceIndices = {0, 0, 1, 2};
    std::vector<uint32_t> expectedSubDeviceIndices = {0, 3, 0, 1};
    for (uint32_t i = 0u; i < devices.size(); i++) {
        std::tuple<uint32_t, uint32_t, uint32_t> subDeviceMap;
        EXPECT_TRUE(executionEnvironment.getSubDeviceHierarchy(i, &subDeviceMap));
        auto hwRootDeviceIndex = std::get<0>(subDeviceMap);
        auto hwSubDeviceIndex = std::get<1>(subDeviceMap);
        auto hwSubDevicesCount = std::get<2>(subDeviceMap);
        EXPECT_EQ(hwRootDeviceIndex, expectedRootDeviceIndices[i]);
        EXPECT_EQ(hwSubDeviceIndex, expectedSubDeviceIndices[i]);
        EXPECT_EQ(hwSubDevicesCount, numSubDevices);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenZeAffinityMaskSetAndTilesAsDevicesModelThenProperSubDeviceIDisReturned) {

    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "FLAT"}};
    debugManager.flags.ZE_AFFINITY_MASK.set("0,1,2,3,"
                                            "5,7,"
                                            "11,"
                                            "12,13,14,15");

    std::vector<uint32_t> expectedRootDeviceIds = {0, 1, 2, 3};
    std::vector<uint32_t> expectedRootDeviceSubDeviceIds = {0, 1, 3, 0};
    std::vector<std::vector<uint32_t>> expectedSubDeviceIdsForAllDevices = {{0, 1, 2, 3}, {1, 3}, {}, {0, 1, 2, 3}};
    std::vector<uint32_t> expectedNumSubDevice = {4, 2, 0, 4};

    debugManager.flags.SetCommandStreamReceiver.set(1);

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    uint32_t expectedRootDevices = numRootDevices;
    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);

    for (uint32_t i = 0; i < devices.size(); i++) {
        auto device = devices[i].get();

        auto rootDeviceId = device->getRootDeviceIndex();
        EXPECT_EQ(expectedRootDeviceIds[i], rootDeviceId);

        auto rootSubDeviceId = SubDevice::getSubDeviceId(*device);
        EXPECT_EQ(expectedRootDeviceSubDeviceIds[i], rootSubDeviceId);

        auto rootNumSubDevices = device->getNumSubDevices();
        EXPECT_EQ(expectedNumSubDevice[i], rootNumSubDevices);

        uint32_t j = 0;
        for (auto &subDevice : device->getSubDevices()) {
            if (subDevice == nullptr) {
                continue;
            }

            auto subDeviceIndex = expectedSubDeviceIdsForAllDevices[i][j];
            EXPECT_EQ(subDeviceIndex, SubDevice::getSubDeviceId(*subDevice));
            j++;
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenZeAffinityMaskSetAndTilesAsDevicesModelThenProperSubDeviceIDsAreReturned) {

    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "FLAT"}};
    debugManager.flags.ZE_AFFINITY_MASK.set("0,1,2,3,"
                                            "5,7,"
                                            "11,"
                                            "12,13,14,15");

    std::vector<uint32_t> expectedRootDeviceIds = {0, 1, 2, 3};
    std::vector<uint32_t> expectedRootDeviceSubDeviceIds = {0, 1, 3, 0};
    std::vector<std::vector<uint32_t>> expectedSubDeviceIdsForAllDevices = {{0, 1, 2, 3}, {1, 3}, {3}, {0, 1, 2, 3}};
    std::vector<uint32_t> expectedNumSubDevice = {4, 2, 0, 4};

    debugManager.flags.SetCommandStreamReceiver.set(1);

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    uint32_t expectedRootDevices = numRootDevices;
    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);

    for (uint32_t i = 0; i < devices.size(); i++) {
        auto device = devices[i].get();

        auto rootDeviceId = device->getRootDeviceIndex();
        EXPECT_EQ(expectedRootDeviceIds[i], rootDeviceId);

        auto rootSubDeviceId = SubDevice::getSubDeviceId(*device);
        EXPECT_EQ(expectedRootDeviceSubDeviceIds[i], rootSubDeviceId);

        auto rootNumSubDevices = device->getNumSubDevices();
        EXPECT_EQ(expectedNumSubDevice[i], rootNumSubDevices);

        auto subDeviceIds = SubDevice::getSubDeviceIdsFromDevice(*device);
        EXPECT_EQ(expectedSubDeviceIdsForAllDevices[i].size(), subDeviceIds.size());
        for (uint32_t j = 0; j < subDeviceIds.size(); j++) {
            EXPECT_EQ(expectedSubDeviceIdsForAllDevices[i][j], subDeviceIds[j]);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenZeAffinityMaskSetThenProperSubDeviceIDisReturned) {

    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.1,0.2,0.3,"
                                            "1.1,1.3,"
                                            "2.3,"
                                            "3.0,3.1,3.2,3.3");

    std::vector<uint32_t> expectedRootDeviceIds = {0, 1, 2, 3};
    std::vector<uint32_t> expectedRootDeviceSubDeviceIds = {0, 1, 3, 0};
    std::vector<std::vector<uint32_t>> expectedSubDeviceIdsForAllDevices = {{0, 1, 2, 3}, {1, 3}, {}, {0, 1, 2, 3}};
    std::vector<uint32_t> expectedNumSubDevice = {4, 2, 0, 4};

    debugManager.flags.SetCommandStreamReceiver.set(1);

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    uint32_t expectedRootDevices = numRootDevices;
    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);

    for (uint32_t i = 0; i < devices.size(); i++) {
        auto device = devices[i].get();

        auto rootDeviceId = device->getRootDeviceIndex();
        EXPECT_EQ(expectedRootDeviceIds[i], rootDeviceId);

        auto rootSubDeviceId = SubDevice::getSubDeviceId(*device);
        EXPECT_EQ(expectedRootDeviceSubDeviceIds[i], rootSubDeviceId);

        auto rootNumSubDevices = device->getNumSubDevices();
        EXPECT_EQ(expectedNumSubDevice[i], rootNumSubDevices);

        uint32_t j = 0;
        for (auto &subDevice : device->getSubDevices()) {
            if (subDevice == nullptr) {
                continue;
            }

            auto subDeviceIndex = expectedSubDeviceIdsForAllDevices[i][j];
            EXPECT_EQ(subDeviceIndex, SubDevice::getSubDeviceId(*subDevice));
            j++;
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenZeAffinityMaskSetThenProperSubDeviceIDsAreReturned) {

    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.1,0.2,0.3,"
                                            "1.1,1.3,"
                                            "2.3,"
                                            "3.0,3.1,3.2,3.3");

    std::vector<uint32_t> expectedRootDeviceIds = {0, 1, 2, 3};
    std::vector<uint32_t> expectedRootDeviceSubDeviceIds = {0, 1, 3, 0};
    std::vector<std::vector<uint32_t>> expectedSubDeviceIdsForAllDevices = {{0, 1, 2, 3}, {1, 3}, {3}, {0, 1, 2, 3}};
    std::vector<uint32_t> expectedNumSubDevice = {4, 2, 0, 4};

    debugManager.flags.SetCommandStreamReceiver.set(1);

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    uint32_t expectedRootDevices = numRootDevices;
    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);

    for (uint32_t i = 0; i < devices.size(); i++) {
        auto device = devices[i].get();

        auto rootDeviceId = device->getRootDeviceIndex();
        EXPECT_EQ(expectedRootDeviceIds[i], rootDeviceId);

        auto rootSubDeviceId = SubDevice::getSubDeviceId(*device);
        EXPECT_EQ(expectedRootDeviceSubDeviceIds[i], rootSubDeviceId);

        auto rootNumSubDevices = device->getNumSubDevices();
        EXPECT_EQ(expectedNumSubDevice[i], rootNumSubDevices);

        auto subDeviceIds = SubDevice::getSubDeviceIdsFromDevice(*device);
        EXPECT_EQ(expectedSubDeviceIdsForAllDevices[i].size(), subDeviceIds.size());
        for (uint32_t j = 0; j < subDeviceIds.size(); j++) {
            EXPECT_EQ(expectedSubDeviceIdsForAllDevices[i][j], subDeviceIds[j]);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZeAffinityMaskSetThenProperSubDeviceHierarchyMapIsSet) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    uint32_t expectedRootDevices = 4;
    debugManager.flags.ZE_AFFINITY_MASK.set("0.2,1.2,2.3,3.3,15,25");

    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);
    std::vector<uint32_t> expectedRootDeviceIndices = {0, 1, 2, 3};
    std::vector<uint32_t> expectedSubDeviceIndices = {2, 2, 3, 3};
    for (uint32_t i = 0u; i < devices.size(); i++) {
        std::tuple<uint32_t, uint32_t, uint32_t> subDeviceMap;
        EXPECT_TRUE(executionEnvironment.getSubDeviceHierarchy(i, &subDeviceMap));
        auto hwRootDeviceIndex = std::get<0>(subDeviceMap);
        auto hwSubDeviceIndex = std::get<1>(subDeviceMap);
        auto hwSubDevicesCount = std::get<2>(subDeviceMap);
        EXPECT_EQ(hwRootDeviceIndex, expectedRootDeviceIndices[i]);
        EXPECT_EQ(hwSubDeviceIndex, expectedSubDeviceIndices[i]);
        EXPECT_EQ(hwSubDevicesCount, numSubDevices);
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZeAffinityMaskSetWithoutTilesThenProperSubDeviceHierarchyMapisUnset) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    uint32_t expectedRootDevices = 4;
    debugManager.flags.ZE_AFFINITY_MASK.set("0,1,2,3,15,25");

    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);
    for (uint32_t i = 0u; i < devices.size(); i++) {
        std::tuple<uint32_t, uint32_t, uint32_t> subDeviceMap;
        EXPECT_FALSE(executionEnvironment.getSubDeviceHierarchy(i, &subDeviceMap));
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZeAffinityMaskSetWhenAllocateRTDispatchGlobalsIsCalledThenRTDispatchGlobalsIsAllocated) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 4;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    uint32_t expectedRootDevices = 4;
    debugManager.flags.ZE_AFFINITY_MASK.set("0.2,1.2,2.3,3.3");

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
    executionEnvironment.incRefInternal();

    auto devices = DeviceFactory::createDevices(executionEnvironment);
    EXPECT_EQ(devices.size(), expectedRootDevices);
    for (uint32_t i = 0u; i < devices.size(); i++) {
        devices[0]->initializeRayTracing(5);
        EXPECT_NE(nullptr, devices[0]->getRTDispatchGlobals(3));
    }
}

TEST_F(DeviceTests, givenDifferentHierarchiesWithoutSubDevicesThenNumSubDevicesIsZero) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);

    auto hwInfo = *defaultHwInfo;

    std::string hierarchies[] = {"COMPOSITE", "FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
        executionEnvironment.incRefInternal();

        auto devices = DeviceFactory::createDevices(executionEnvironment);
        EXPECT_EQ(devices.size(), numRootDevices);

        for (uint32_t i = 0u; i < devices.size(); i++) {
            EXPECT_EQ(devices[i]->getNumSubDevices(), 0u);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZeAffinityMaskSetWithDifferentHierarchiesThenNumSubDevicesIsCorrect) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    uint32_t numRootDevices = 4;
    uint32_t numSubDevices = 2;
    uint32_t expectedNumSubDevices;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    debugManager.flags.ZE_AFFINITY_MASK.set("0,2");

    auto hwInfo = *defaultHwInfo;

    std::string hierarchies[] = {"COMPOSITE", "FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        MockExecutionEnvironment executionEnvironment(&hwInfo, false, numRootDevices);
        executionEnvironment.incRefInternal();

        auto devices = DeviceFactory::createDevices(executionEnvironment);
        EXPECT_EQ(devices.size(), 2u);

        if (hierarchy == "COMPOSITE") {
            expectedNumSubDevices = 2u;
        } else if (hierarchy == "FLAT") {
            expectedNumSubDevices = 0u;
        } else if (hierarchy == "COMBINED") {
            expectedNumSubDevices = 1u;
        }

        for (uint32_t i = 0u; i < devices.size(); i++) {
            EXPECT_EQ(devices[i]->getNumSubDevices(), expectedNumSubDevices);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZexNumberOfCssEnvVariableIsLargerThanNumberOfAvailableCcsCountWhenDeviceIsCreatedThenCreateDevicesWithAvailableCcsCount) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:13");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto device = deviceFactory.rootDevices[0];

    auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
    auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
    EXPECT_EQ(defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled, computeEngineGroup.engines.size());
}

HWCMDTEST_F(IGFX_XE_HPC_CORE, DeviceTests, givenZexNumberOfCssEnvVariableSetAmbigouslyWhenDeviceIsCreatedThenDontApplyAnyLimitations) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(1);
    for (const auto &numberOfCcsString : {"default", "", "0"}) {
        debugManager.flags.ZEX_NUMBER_OF_CCS.set(numberOfCcsString);

        auto hwInfo = *defaultHwInfo;

        MockExecutionEnvironment executionEnvironment(&hwInfo);
        executionEnvironment.incRefInternal();

        UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

        auto device = deviceFactory.rootDevices[0];

        auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
        auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
        EXPECT_EQ(defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled, computeEngineGroup.engines.size());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenDebuggableOsContextWhenDeviceCreatesEnginesThenDeviceIsInitializedWithFirstSubmission) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useFirstSubmissionInitDevice = true;

    auto hwInfo = *defaultHwInfo;
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    hardwareInfoSetup[hwInfo.platform.eProductFamily](&hwInfo, true, 0, releaseHelper.get());

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.memoryManager.reset(new MockMemoryManagerWithDebuggableOsContext(executionEnvironment));
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto device = deviceFactory.rootDevices[0];
    auto csr = device->allEngines[device->defaultEngineIndex].commandStreamReceiver;
    EXPECT_EQ(1u, csr->peekLatestSentTaskCount());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, whenDeviceCreatesEnginesThenDeviceIsInitializedWithFirstSubmission) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useFirstSubmissionInitDevice = true;

    auto hwInfo = *defaultHwInfo;
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    hardwareInfoSetup[hwInfo.platform.eProductFamily](&hwInfo, true, 0, releaseHelper.get());

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto device = deviceFactory.rootDevices[0];
    auto csr = device->allEngines[device->defaultEngineIndex].commandStreamReceiver;

    if (device->isInitDeviceWithFirstSubmissionSupported(csr->getType())) {
        EXPECT_EQ(1u, csr->peekLatestSentTaskCount());
    }
}

TEST(FailDeviceTest, GivenFailedDeviceWhenCreatingDeviceThenNullIsReturned) {
    auto hwInfo = defaultHwInfo.get();
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDevice>(hwInfo);

    EXPECT_EQ(nullptr, pDevice);
}

TEST(FailDeviceTest, GivenMidThreadPreemptionAndFailedDeviceWhenCreatingDeviceThenNullIsReturned) {
    VariableBackup<bool> backupSipInitType(&MockSipData::useMockSip, true);
    VariableBackup<bool> mockSipCalled(&NEO::MockSipData::called, false);
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDeviceAfterOne>(defaultHwInfo.get());

    EXPECT_EQ(nullptr, pDevice);
}

TEST_F(DeviceTests, givenDeviceMidThreadPreemptionWhenDebuggerDisabledThenStateSipRequired) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    device->setPreemptionMode(NEO::PreemptionMode::MidThread);
    EXPECT_TRUE(device->isStateSipRequired());
}

TEST_F(DeviceTests, givenDeviceThreadGroupPreemptionWhenDebuggerEnabledThenStateSipRequired) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->setPreemptionMode(NEO::PreemptionMode::ThreadGroup);
    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(device.get());
    EXPECT_TRUE(device->isStateSipRequired());
}

TEST_F(DeviceTests, givenDeviceThreadGroupPreemptionWhenDebuggerDisabledThenStateSipNotRequired) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->setPreemptionMode(NEO::PreemptionMode::ThreadGroup);
    EXPECT_FALSE(device->isStateSipRequired());
}

TEST_F(DeviceTests, WhenIsStateSipRequiredIsCalledThenCorrectValueIsReturned) {
    struct MockRootDeviceEnvironment : RootDeviceEnvironment {
        using RootDeviceEnvironment::RootDeviceEnvironment;
        CompilerInterface *getCompilerInterface() override {
            return compilerInterfaceReturnValue;
        }
        CompilerInterface *compilerInterfaceReturnValue = nullptr;
    };
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto mockRootDeviceEnvironment = new MockRootDeviceEnvironment{*device->executionEnvironment};
    auto backupenv = device->executionEnvironment->rootDeviceEnvironments[0].release();
    device->executionEnvironment->rootDeviceEnvironments[0].reset(mockRootDeviceEnvironment);
    device->executionEnvironment->rootDeviceEnvironments[0]->compilerInterface.release();
    device->executionEnvironment->rootDeviceEnvironments[0]->debugger.release();

    std::array<std::tuple<PreemptionMode, Debugger *, CompilerInterface *, bool>, 8> testParameters = {
        {{PreemptionMode::Disabled, nullptr, nullptr, false},
         {PreemptionMode::MidThread, nullptr, nullptr, false},
         {PreemptionMode::Disabled, (Debugger *)0x1234, nullptr, false},
         {PreemptionMode::MidThread, (Debugger *)0x1234, nullptr, false},

         {PreemptionMode::Disabled, nullptr, (CompilerInterface *)0x1234, false},
         {PreemptionMode::MidThread, nullptr, (CompilerInterface *)0x1234, true},
         {PreemptionMode::Disabled, (Debugger *)0x1234, (CompilerInterface *)0x1234, true},
         {PreemptionMode::MidThread, (Debugger *)0x1234, (CompilerInterface *)0x1234, true}}};

    for (const auto &[preemptionMode, debugger, compilerInterface, expectedResult] : testParameters) {
        device->setPreemptionMode(preemptionMode);
        device->executionEnvironment->rootDeviceEnvironments[0]->debugger.release();
        device->executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(debugger);
        mockRootDeviceEnvironment->compilerInterfaceReturnValue = compilerInterface;

        EXPECT_EQ(expectedResult, device->isStateSipRequired());
    }
    device->executionEnvironment->rootDeviceEnvironments[0]->debugger.release();
    mockRootDeviceEnvironment->compilerInterfaceReturnValue = nullptr;
    delete device->executionEnvironment->rootDeviceEnvironments[0].release();
    device->executionEnvironment->rootDeviceEnvironments[0].reset(backupenv);
}

HWTEST2_F(DeviceTests, GivenXeHpAndLaterThenDefaultPreemptionModeIsThreadGroup, IsXeCore) {
    EXPECT_EQ(PreemptionMode::ThreadGroup, defaultHwInfo->capabilityTable.defaultPreemptionMode);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTest, GivenXeHpAndLaterThenProfilingTimerResolutionIs83) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, GivenXeHpAndLaterThenKmdNotifyIsDisabled) {
    EXPECT_FALSE(defaultHwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(0, defaultHwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_FALSE(defaultHwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(0, defaultHwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_FALSE(defaultHwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(0, defaultHwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    EXPECT_FALSE(defaultHwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(0, defaultHwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, GivenXeHpAndLaterThenCompressionFeatureFlagIsFalse) {
    EXPECT_FALSE(defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(defaultHwInfo->capabilityTable.ftrRenderCompressedImages);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTest, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    auto &productHelper = pDevice->getProductHelper();

    const uint32_t multiplyFactor = productHelper.getThreadEuRatioForScratch(hwInfo) / 8u;
    const uint32_t numThreadsPerEu = hwInfo.gtSystemInfo.NumThreadsPerEu * multiplyFactor;

    uint32_t expectedValue = productHelper.computeMaxNeededSubSliceSpace(hwInfo) * hwInfo.gtSystemInfo.MaxEuPerSubSlice * numThreadsPerEu;

    EXPECT_EQ(expectedValue, gfxCoreHelper.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment()));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenDebugFlagSetWhenAskingForComputeUnitsForScratchThenReturnNewValue) {
    DebugManagerStateRestore restore;
    uint32_t expectedValue = defaultHwInfo->gtSystemInfo.ThreadCount + 11;
    debugManager.flags.OverrideNumComputeUnitsForScratch.set(static_cast<int32_t>(expectedValue));

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    EXPECT_EQ(expectedValue, gfxCoreHelper.getComputeUnitsUsedForScratch(device->getRootDeviceEnvironment()));
    EXPECT_EQ(expectedValue, device->getDeviceInfo().computeUnitsUsedForScratch);
}

HWTEST2_F(DeviceTests, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue, IsXeHpgCore) {
    EXPECT_EQ(64u, defaultHwInfo->capabilityTable.maxProgrammableSlmSize);
}

TEST_F(DeviceTests, whenCheckingPreferredPlatformNameThenNullIsReturned) {
    EXPECT_EQ(nullptr, defaultHwInfo->capabilityTable.preferredPlatformName);
}

TEST(Device, givenDifferentEngineTypesWhenIsSecondaryContextEngineTypeCalledThenTrueReturnedForCcsOrBcs) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    for (uint32_t i = 0; i < static_cast<uint32_t>(aub_stream::EngineType::NUM_ENGINES); i++) {
        auto type = static_cast<aub_stream::EngineType>(i);

        if (EngineHelpers::isBcs(type) || EngineHelpers::isCcs(type)) {
            EXPECT_TRUE(device->isSecondaryContextEngineType(type));
        } else {
            EXPECT_FALSE(device->isSecondaryContextEngineType(type));
        }
    }
}

TEST(Device, whenAllocateDebugSurfaceIsCalledThenEachSubDeviceContainsCorrectDebugSurface) {

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.CreateMultipleSubDevices.set(4);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    size_t size = 8u;
    device->allocateDebugSurface(size);
    auto *debugSurface = device->getDebugSurface();

    for (auto *subDevice : device->getSubDevices()) {
        EXPECT_EQ(debugSurface, subDevice->getDebugSurface());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTests, givenCCSEngineAndContextGroupSizeEnabledWhenCreatingEngineThenItsContextHasContextGroupFlagSet) {
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 8;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto defaultEngine = deviceFactory.rootDevices[0]->getDefaultEngine();
    EXPECT_NE(nullptr, &defaultEngine);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, defaultEngine.getEngineType());
    EXPECT_EQ(EngineUsage::regular, defaultEngine.getEngineUsage());

    EXPECT_TRUE(defaultEngine.osContext->isPartOfContextGroup());
}

HWTEST_F(DeviceTests, givenCCSEnginesAndContextGroupSizeEnabledWhenDeviceIsCreatedThenSecondaryEnginesAreCreated) {
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 8;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    uint32_t numOfCCS[] = {1, 2, 4};
    for (size_t i = 0; i < arrayCount(numOfCCS); i++) {

        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = numOfCCS[i];

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        auto &engineGroups = device->getRegularEngineGroups();

        auto engineGroupType = EngineGroupType::compute;
        size_t computeEnginesCount = 0;
        for (const auto &engine : engineGroups) {
            if (engine.engineGroupType == engineGroupType) {
                computeEnginesCount = engine.engines.size();
            }
        }

        if (computeEnginesCount == 0) {
            GTEST_SKIP();
        }

        ASSERT_EQ(computeEnginesCount, device->secondaryEngines.size());
        ASSERT_EQ(contextGroupSize / numOfCCS[i], device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

        auto defaultEngine = device->getDefaultEngine();
        EXPECT_EQ(defaultEngine.commandStreamReceiver, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines[0].commandStreamReceiver);

        const uint32_t regularContextCount = std::min(contextGroupSize / 2, 4u) / numOfCCS[i];

        for (uint32_t ccsIndex = 0; ccsIndex < computeEnginesCount; ccsIndex++) {
            auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];

            EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
            EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

            for (size_t i = 1; i < device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size(); i++) {
                EXPECT_EQ(secondaryEngines.engines[0].osContext, secondaryEngines.engines[i].osContext->getPrimaryContext());
                EXPECT_TRUE(secondaryEngines.engines[i].osContext->isPartOfContextGroup());
            }

            EXPECT_EQ(0u, secondaryEngines.regularCounter.load());
            EXPECT_EQ(0u, secondaryEngines.highPriorityCounter.load());

            EXPECT_EQ(regularContextCount, secondaryEngines.regularEnginesTotal);
            EXPECT_EQ(contextGroupSize / numOfCCS[i] - regularContextCount, secondaryEngines.highPriorityEnginesTotal);

            for (size_t contextId = 0; contextId < regularContextCount + 1; contextId++) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(contextId + 1, secondaryEngines.regularCounter.load());
                if (contextId == regularContextCount) {
                    EXPECT_EQ(&secondaryEngines.engines[0], engine);
                }
            }

            auto hpCount = contextGroupSize / numOfCCS[i] - regularContextCount;
            for (size_t contextId = 0; contextId < hpCount + 1; contextId++) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(contextId + 1, secondaryEngines.highPriorityCounter.load());
                if (contextId == hpCount) {
                    EXPECT_EQ(&secondaryEngines.engines[regularContextCount], engine);
                }
            }
        }

        auto internalEngine = device->getInternalEngine();
        EXPECT_NE(internalEngine.commandStreamReceiver, device->getSecondaryEngineCsr({aub_stream::EngineType::ENGINE_CCS, EngineUsage::internal}, 0, false)->commandStreamReceiver);
    }
}

HWTEST_F(DeviceTests, givenExposeSingleDeviceAndContextGroupSizeEnabledWhenRootDeviceIsCreatedThenBcsEnginesAreCreatedInSubDevices) {
    if (defaultHwInfo->capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 8;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);
    debugManager.flags.CreateMultipleSubDevices.set(2);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0);
    executionEnvironment->rootDeviceEnvironments[0]->setExposeSingleDeviceMode(true);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

    {
        auto mockSubDevice = static_cast<MockSubDevice *>(device->subdevices[0]);

        ASSERT_EQ(0u, device->secondaryEngines[aub_stream::EngineType::ENGINE_BCS].engines.size());

        auto copyEngineCount = mockSubDevice->tryGetRegularEngineGroup(EngineGroupType::copy)->engines.size();
        ASSERT_NE(0u, copyEngineCount);

        auto &secondaryEngines = mockSubDevice->secondaryEngines[mockSubDevice->tryGetRegularEngineGroup(EngineGroupType::copy)->engines[0].getEngineType()];

        EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
        EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

        for (size_t i = 1; i < secondaryEngines.engines.size(); i++) {
            EXPECT_EQ(secondaryEngines.engines[0].osContext, secondaryEngines.engines[i].osContext->getPrimaryContext());
            EXPECT_TRUE(secondaryEngines.engines[i].osContext->isPartOfContextGroup());
            EXPECT_FALSE(secondaryEngines.engines[i].osContext->isRootDevice());
            EXPECT_EQ(1u, secondaryEngines.engines[i].osContext->getDeviceBitfield().count());
            EXPECT_TRUE(secondaryEngines.engines[i].osContext->getDeviceBitfield().test(0));
        }
    }

    {
        auto mockSubDevice2 = static_cast<MockSubDevice *>(device->subdevices[1]);

        ASSERT_EQ(0u, device->secondaryEngines[aub_stream::EngineType::ENGINE_BCS].engines.size());

        auto copyEngineCount = mockSubDevice2->tryGetRegularEngineGroup(EngineGroupType::copy)->engines.size();
        ASSERT_NE(0u, copyEngineCount);

        auto &secondaryEngines = mockSubDevice2->secondaryEngines[mockSubDevice2->tryGetRegularEngineGroup(EngineGroupType::copy)->engines[0].getEngineType()];

        EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
        EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

        for (size_t i = 1; i < secondaryEngines.engines.size(); i++) {
            EXPECT_EQ(secondaryEngines.engines[0].osContext, secondaryEngines.engines[i].osContext->getPrimaryContext());
            EXPECT_TRUE(secondaryEngines.engines[i].osContext->isPartOfContextGroup());
            EXPECT_FALSE(secondaryEngines.engines[i].osContext->isRootDevice());
            EXPECT_EQ(1u, secondaryEngines.engines[i].osContext->getDeviceBitfield().count());
            EXPECT_TRUE(secondaryEngines.engines[i].osContext->getDeviceBitfield().test(1));
        }
    }
}

HWTEST_F(DeviceTests, givenRootDeviceWithCCSEngineAndContextGroupSizeEnabledWhenDeviceIsCreatedThenSecondaryEnginesAreCreated) {
    if (defaultHwInfo->capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 8;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);
    debugManager.flags.CreateMultipleSubDevices.set(2);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    bool singleDeviceMode[] = {false, true};

    for (auto &singleDevMode : singleDeviceMode) {
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0);

        if (singleDevMode) {
            executionEnvironment->rootDeviceEnvironments[0]->setExposeSingleDeviceMode(true);
        }
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));
        auto &engineGroups = device->getRegularEngineGroups();

        auto engineGroupType = EngineGroupType::compute;
        size_t computeEnginesCount = 0;
        for (const auto &engine : engineGroups) {
            if (engine.engineGroupType == engineGroupType) {
                computeEnginesCount = engine.engines.size();
            }
        }

        if (computeEnginesCount == 0) {
            GTEST_SKIP();
        }

        ASSERT_EQ(computeEnginesCount, device->secondaryEngines.size());
        ASSERT_EQ(contextGroupSize, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

        auto defaultEngine = device->getDefaultEngine();
        EXPECT_EQ(defaultEngine.commandStreamReceiver, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines[0].commandStreamReceiver);

        const uint32_t regularContextCount = std::min(contextGroupSize / 2, 4u);

        for (uint32_t ccsIndex = 0; ccsIndex < computeEnginesCount; ccsIndex++) {
            auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];

            EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
            EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

            for (size_t i = 1; i < device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size(); i++) {
                EXPECT_EQ(secondaryEngines.engines[0].osContext, secondaryEngines.engines[i].osContext->getPrimaryContext());
                EXPECT_TRUE(secondaryEngines.engines[i].osContext->isPartOfContextGroup());
                EXPECT_TRUE(secondaryEngines.engines[i].osContext->isRootDevice());
            }

            EXPECT_EQ(0u, secondaryEngines.regularCounter.load());
            EXPECT_EQ(0u, secondaryEngines.highPriorityCounter.load());

            EXPECT_EQ(regularContextCount, secondaryEngines.regularEnginesTotal);
            EXPECT_EQ(contextGroupSize - regularContextCount, secondaryEngines.highPriorityEnginesTotal);

            for (size_t contextId = 0; contextId < regularContextCount + 1; contextId++) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(contextId + 1, secondaryEngines.regularCounter.load());
                if (contextId == regularContextCount) {
                    EXPECT_EQ(&secondaryEngines.engines[0], engine);
                }
            }

            for (size_t contextId = 0; contextId < contextGroupSize - regularContextCount + 1; contextId++) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(contextId + 1, secondaryEngines.highPriorityCounter.load());
                if (contextId == contextGroupSize - regularContextCount) {
                    EXPECT_EQ(&secondaryEngines.engines[regularContextCount], engine);
                }
            }
        }
    }
}

HWTEST_F(DeviceTests, givenRootDeviceAndDebugFlagSetWhenCreatingSecondaryEnginesThenCreateCorrectNumberOfHighPriorityContexts) {
    if (defaultHwInfo->capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 8;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.OverrideNumHighPriorityContexts.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const uint32_t regularContextCount = 7u;

    uint32_t ccsIndex = 0;
    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];

    EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
    EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

    EXPECT_EQ(0u, secondaryEngines.regularCounter.load());
    EXPECT_EQ(0u, secondaryEngines.highPriorityCounter.load());

    EXPECT_EQ(regularContextCount, secondaryEngines.regularEnginesTotal);
    EXPECT_EQ(contextGroupSize - regularContextCount, secondaryEngines.highPriorityEnginesTotal);
}

HWTEST_F(DeviceTests, givenContextGroupSizeEnabledWhenMoreHpEnginesCreatedThenFreeEnginesAreAssignedUpToHalfOfContextGroup) {
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 14;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    auto &engineGroups = device->getRegularEngineGroups();

    auto engineGroupType = EngineGroupType::compute;
    size_t computeEnginesCount = 0;
    for (const auto &engine : engineGroups) {
        if (engine.engineGroupType == engineGroupType) {
            computeEnginesCount = engine.engines.size();
        }
    }

    if (computeEnginesCount == 0) {
        GTEST_SKIP();
    }

    ASSERT_EQ(computeEnginesCount, device->secondaryEngines.size());
    ASSERT_EQ(contextGroupSize, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

    auto defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(defaultEngine.commandStreamReceiver, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines[0].commandStreamReceiver);

    const uint32_t maxHpContextCount = contextGroupSize / 2;

    for (uint32_t ccsIndex = 0; ccsIndex < computeEnginesCount; ccsIndex++) {
        auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];

        EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
        EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

        for (size_t i = 1; i < device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size(); i++) {
            EXPECT_EQ(secondaryEngines.engines[0].osContext, secondaryEngines.engines[i].osContext->getPrimaryContext());
            EXPECT_TRUE(secondaryEngines.engines[i].osContext->isPartOfContextGroup());
        }

        EXPECT_EQ(0u, secondaryEngines.regularCounter.load());
        EXPECT_EQ(0u, secondaryEngines.highPriorityCounter.load());

        auto regularContextCount = secondaryEngines.regularEnginesTotal;
        EXPECT_EQ(contextGroupSize - regularContextCount, secondaryEngines.highPriorityEnginesTotal);

        uint32_t npCounter = 0;
        uint32_t hpCounter = 0;
        std::vector<EngineControl *> hpEngines;

        for (size_t contextId = 0; contextId < maxHpContextCount + 2; contextId++) {

            if (contextId == 2) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(1, secondaryEngines.regularCounter.load());
                EXPECT_EQ(&secondaryEngines.engines[npCounter], engine);
                EXPECT_FALSE(secondaryEngines.engines[npCounter].osContext->isHighPriority());

                npCounter++;
            }
            if (contextId == 6) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(2, secondaryEngines.regularCounter.load());
                EXPECT_EQ(&secondaryEngines.engines[npCounter], engine);
                EXPECT_FALSE(secondaryEngines.engines[npCounter].osContext->isHighPriority());

                npCounter++;
            }

            auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
            ASSERT_NE(nullptr, engine);
            hpEngines.push_back(engine);
            hpCounter++;

            if (contextId < secondaryEngines.highPriorityEnginesTotal) {
                EXPECT_EQ(&secondaryEngines.engines[regularContextCount + hpCounter - 1], engine);
                EXPECT_TRUE(secondaryEngines.engines[regularContextCount + hpCounter - 1].osContext->isHighPriority());
            } else if (contextId >= secondaryEngines.highPriorityEnginesTotal) {

                if (hpCounter <= maxHpContextCount) {
                    EXPECT_EQ(&secondaryEngines.engines[npCounter], engine);
                    EXPECT_TRUE(secondaryEngines.engines[npCounter].osContext->isHighPriority());
                    npCounter++;
                } else {
                    EXPECT_EQ(hpEngines[hpCounter - 1 % maxHpContextCount], engine);
                    EXPECT_TRUE(hpEngines[hpCounter - 1 % maxHpContextCount]->osContext->isHighPriority());
                }
            }
        }
    }
}

HWTEST_F(DeviceTests, givenDeviceWithCCSEngineAndAggregatedProcessesWhenDeviceIsCreatedThenNumberOfSecondaryEnginesIsLimited) {
    if (defaultHwInfo->capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore dbgRestorer;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;

    const auto numProcesses = 4u;
    auto executionEnvironment = new MockExecutionEnvironment(&hwInfo, false, 1);
    executionEnvironment->incRefInternal();
    auto osInterface = new MockOsInterface();
    auto driverModelMock = std::make_unique<MockDriverModel>();
    osInterface->setDriverModel(std::move(driverModelMock));

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    osInterface->numberOfProcesses = numProcesses;

    uint32_t testedContextGroupSizes[] = {10, 22};
    uint32_t expectedRegularCounts[] = {1u, 3u};
    uint32_t expectedHpCounts[] = {1u, 2u};

    for (uint32_t contextGroupSizeIndex = 0; contextGroupSizeIndex < 2; contextGroupSizeIndex++) {
        debugManager.flags.ContextGroupSize.set(testedContextGroupSizes[contextGroupSizeIndex]);

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));
        auto &engineGroups = device->getRegularEngineGroups();

        auto engineGroupType = EngineGroupType::compute;
        size_t computeEnginesCount = 0;
        for (const auto &engine : engineGroups) {
            if (engine.engineGroupType == engineGroupType) {
                computeEnginesCount = engine.engines.size();
            }
        }

        ASSERT_EQ(computeEnginesCount, device->secondaryEngines.size());
        ASSERT_EQ(testedContextGroupSizes[contextGroupSizeIndex] / numProcesses, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

        auto defaultEngine = device->getDefaultEngine();
        EXPECT_EQ(defaultEngine.commandStreamReceiver, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines[0].commandStreamReceiver);

        const uint32_t regularContextCount = expectedRegularCounts[contextGroupSizeIndex];
        const uint32_t hpContextCount = expectedHpCounts[contextGroupSizeIndex];

        for (uint32_t ccsIndex = 0; ccsIndex < computeEnginesCount; ccsIndex++) {
            auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];

            EXPECT_TRUE(secondaryEngines.engines[0].osContext->isPartOfContextGroup());
            EXPECT_EQ(nullptr, secondaryEngines.engines[0].osContext->getPrimaryContext());

            for (size_t i = 1; i < device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size(); i++) {
                EXPECT_EQ(secondaryEngines.engines[0].osContext, secondaryEngines.engines[i].osContext->getPrimaryContext());
                EXPECT_TRUE(secondaryEngines.engines[i].osContext->isPartOfContextGroup());
            }

            EXPECT_EQ(regularContextCount, secondaryEngines.regularEnginesTotal);
            EXPECT_EQ(hpContextCount, secondaryEngines.highPriorityEnginesTotal);

            for (size_t contextId = 0; contextId < regularContextCount + 1; contextId++) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
                ASSERT_NE(nullptr, engine);

                if (contextId == regularContextCount) {
                    EXPECT_EQ(&secondaryEngines.engines[0], engine);
                }
            }

            for (size_t contextId = 0; contextId < hpContextCount; contextId++) {
                auto engine = device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
                ASSERT_NE(nullptr, engine);

                EXPECT_EQ(contextId + 1, secondaryEngines.highPriorityCounter.load());
                if (contextId == testedContextGroupSizes[contextGroupSizeIndex] - regularContextCount) {
                    EXPECT_EQ(&secondaryEngines.engines[regularContextCount], engine);
                }
            }
        }
        auto memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());
        EXPECT_GT(memoryManager->maxOsContextCount, memoryManager->latestContextId);
        executionEnvironment->memoryManager->reInitLatestContextId();
    }
    executionEnvironment->decRefInternal();
}

HWTEST_F(DeviceTests, givenRootDeviceAndAggregatedProcessesWhenDeviceIsCreatedThenSecondaryEnginesCountIsLimited) {
    if (defaultHwInfo->capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore dbgRestorer;
    const uint32_t contextGroupSize = 12;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);
    debugManager.flags.CreateMultipleSubDevices.set(2);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    const auto numProcesses = 4u;
    bool singleDeviceMode[] = {false, true};

    for (auto &exposeDeviceMode : singleDeviceMode) {
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0);
        auto osInterface = new MockOsInterface();
        auto driverModelMock = std::make_unique<MockDriverModel>();
        osInterface->setDriverModel(std::move(driverModelMock));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        osInterface->numberOfProcesses = numProcesses;

        if (exposeDeviceMode) {
            executionEnvironment->rootDeviceEnvironments[0]->setExposeSingleDeviceMode(true);
        }

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));
        auto &secondaryEngines = device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];

        ASSERT_EQ(3u, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());
        EXPECT_EQ(2u, secondaryEngines.regularEnginesTotal);
        EXPECT_EQ(1u, secondaryEngines.highPriorityEnginesTotal);
    }
}

HWTEST_F(DeviceTests, givenDebugFlagSetWhenCreatingSecondaryEnginesThenCreateCorrectNumberOfHighPriorityContexts) {
    DebugManagerStateRestore dbgRestorer;
    constexpr uint32_t contextGroupSize = 16;
    constexpr uint32_t numHighPriorityContexts = 6;
    debugManager.flags.ContextGroupSize.set(contextGroupSize);
    debugManager.flags.OverrideNumHighPriorityContexts.set(numHighPriorityContexts);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    {
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        auto &engineGroups = device->getRegularEngineGroups();

        auto engineGroupType = EngineGroupType::compute;
        size_t computeEnginesCount = 0;
        for (const auto &engine : engineGroups) {
            if (engine.engineGroupType == engineGroupType) {
                computeEnginesCount = engine.engines.size();
            }
        }

        if (computeEnginesCount == 0) {
            GTEST_SKIP();
        }

        ASSERT_EQ(computeEnginesCount, device->secondaryEngines.size());
        ASSERT_EQ(contextGroupSize, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

        constexpr uint32_t regularContextCount = contextGroupSize - numHighPriorityContexts;

        auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(0)];

        EXPECT_EQ(regularContextCount, secondaryEngines.regularEnginesTotal);
        EXPECT_EQ(contextGroupSize - regularContextCount, secondaryEngines.highPriorityEnginesTotal);
    }
    {
        debugManager.flags.OverrideNumHighPriorityContexts.set(0);
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

        ASSERT_EQ(contextGroupSize, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

        auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(0)];

        EXPECT_EQ(nullptr, secondaryEngines.getEngine(EngineUsage::highPriority, 0));
    }
    {
        debugManager.flags.OverrideNumHighPriorityContexts.set(contextGroupSize);
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

        ASSERT_EQ(contextGroupSize, device->secondaryEngines[aub_stream::EngineType::ENGINE_CCS].engines.size());

        auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(0)];

        EXPECT_EQ(nullptr, secondaryEngines.getEngine(EngineUsage::regular, 0));
    }
}

HWTEST_F(DeviceTests, givenContextGroupEnabledWhenGettingSecondaryEngineThenResourcesAndContextAreInitialized) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    const auto ccsIndex = 0;

    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];

    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);

    EXPECT_TRUE(secondaryEngines.engines[0].commandStreamReceiver->isInitialized());
    EXPECT_EQ(1u, secondaryEngines.engines[0].commandStreamReceiver->peekLatestSentTaskCount());

    auto primaryCsr = secondaryEngines.engines[0].commandStreamReceiver;
    for (uint32_t secondaryIndex = 1; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {

        EXPECT_FALSE(secondaryEngines.engines[secondaryIndex].osContext->isInitialized());
        EXPECT_FALSE(secondaryEngines.engines[secondaryIndex].commandStreamReceiver->isInitialized());

        EXPECT_EQ(nullptr, secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getTagAllocation());
        EXPECT_EQ(primaryCsr->getGlobalFenceAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalFenceAllocation());
        if (device->getPreemptionMode() == PreemptionMode::MidThread) {
            EXPECT_EQ(primaryCsr->getPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionAllocation());
        }

        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
    }

    for (uint32_t i = 0; i < secondaryEngines.highPriorityEnginesTotal; i++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
    }

    for (uint32_t secondaryIndex = 0; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {

        EXPECT_TRUE(secondaryEngines.engines[secondaryIndex].osContext->isInitialized());
        EXPECT_TRUE(secondaryEngines.engines[secondaryIndex].commandStreamReceiver->isInitialized());

        EXPECT_NE(nullptr, secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getTagAllocation());

        if (gfxCoreHelper.isFenceAllocationRequired(hwInfo, device->getRootDeviceEnvironment().getHelper<ProductHelper>())) {
            EXPECT_NE(nullptr, secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalFenceAllocation());
        }
        if (device->getPreemptionMode() == PreemptionMode::MidThread) {
            EXPECT_NE(nullptr, secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionAllocation());
        }
    }
}

HWTEST_F(DeviceTests, givenContextGroupEnabledWhenDeviceIsDestroyedThenSecondaryContextsAreReleased) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->incRefInternal();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));
    auto memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());

    const auto ccsIndex = 0;
    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];
    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);
    ASSERT_LE(1u, memoryManager->secondaryEngines.size());
    EXPECT_EQ(secondaryEnginesCount - 1, memoryManager->secondaryEngines[0].size());

    device.reset(nullptr);

    EXPECT_EQ(0u, memoryManager->secondaryEngines[0].size());
    executionEnvironment->decRefInternal();
}

HWTEST_F(DeviceTests, givenContextGroupEnabledAndAllocationUsedBySeconadryContextWhenDeviceIsDestroyedThenNotCompletedAllocationsAreWaitedOn) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->incRefInternal();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));
    auto memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());

    EXPECT_NE(device->secondaryEngines.end(), device->secondaryEngines.find(aub_stream::ENGINE_CCS));
    auto &secondaryEngines = device->secondaryEngines[aub_stream::ENGINE_CCS];
    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);

    auto engine = device->getSecondaryEngineCsr({aub_stream::ENGINE_CCS, EngineUsage::regular}, 0, false);
    ASSERT_NE(nullptr, engine);
    auto csr = engine->commandStreamReceiver;
    auto engine2 = device->getSecondaryEngineCsr({aub_stream::ENGINE_CCS, EngineUsage::regular}, 0, false);
    ASSERT_NE(nullptr, engine2);
    auto csr2 = engine2->commandStreamReceiver;
    ASSERT_NE(csr, csr2);
    auto tagAddress = csr->getTagAddress();
    auto tagAddress2 = csr2->getTagAddress();

    EXPECT_NE(csr->getOsContext().getContextId(), csr2->getOsContext().getContextId());
    EXPECT_NE(tagAddress, tagAddress2);

    auto usedAllocationAndNotGpuCompleted = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    usedAllocationAndNotGpuCompleted->updateTaskCount(*tagAddress + 1, csr->getOsContext().getContextId());

    auto usedAllocationAndNotGpuCompleted2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    usedAllocationAndNotGpuCompleted2->updateTaskCount(*tagAddress2 + 1, csr2->getOsContext().getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationAndNotGpuCompleted);
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationAndNotGpuCompleted2);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(csr->getDeferredAllocations().peekIsEmpty());
    EXPECT_EQ(csr->getDeferredAllocations().peekHead(), usedAllocationAndNotGpuCompleted);

    usedAllocationAndNotGpuCompleted->updateTaskCount(csr->peekLatestFlushedTaskCount(), csr->getOsContext().getContextId());
    usedAllocationAndNotGpuCompleted2->updateTaskCount(csr2->peekLatestFlushedTaskCount(), csr2->getOsContext().getContextId());

    device.reset(nullptr);

    EXPECT_EQ(0u, memoryManager->secondaryEngines[0].size());
    EXPECT_EQ(0u, memoryManager->allRegisteredEngines[0].size());
    executionEnvironment->decRefInternal();
}

HWTEST_F(DeviceTests, givenCopyEnginesWhenCreatingSecondaryContextsThenUseCopyTypes) {
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 0b1111;

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->incRefInternal();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));
    auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    auto memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());

    auto &enabledEngines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    for (auto engineType : {aub_stream::EngineType::ENGINE_BCS, aub_stream::EngineType::ENGINE_BCS1, aub_stream::EngineType::ENGINE_BCS2, aub_stream::EngineType::ENGINE_BCS3}) {
        auto supportedRegular = std::find_if(enabledEngines.begin(), enabledEngines.end(),
                                             [&engineType](const auto &engine) { return (engine.first == engineType) && (engine.second == EngineUsage::regular); }) != enabledEngines.end();
        auto supportedHp = std::find_if(enabledEngines.begin(), enabledEngines.end(),
                                        [&engineType](const auto &engine) { return (engine.first == engineType) && (engine.second == EngineUsage::highPriority); }) != enabledEngines.end();

        if (supportedRegular || supportedHp) {
            auto usage = supportedRegular ? EngineUsage::regular : EngineUsage::highPriority;
            EXPECT_NE(device->secondaryEngines.end(), device->secondaryEngines.find(engineType));

            auto expectedEngineCount = 5u;
            if (supportedRegular) {
                gfxCoreHelper.adjustCopyEngineRegularContextCount(device->secondaryEngines[engineType].engines.size(), expectedEngineCount);
            }

            EXPECT_EQ(expectedEngineCount, device->secondaryEngines[engineType].engines.size());

            auto engine = device->getSecondaryEngineCsr({engineType, usage}, 0, false);
            ASSERT_NE(nullptr, engine);

            auto csr = engine->commandStreamReceiver;
            auto engine2 = device->getSecondaryEngineCsr({engineType, usage}, 0, false);
            ASSERT_NE(nullptr, engine2);

            auto csr2 = engine2->commandStreamReceiver;
            ASSERT_NE(csr, csr2);

            auto tagAddress = csr->getTagAddress();
            auto tagAddress2 = csr2->getTagAddress();

            EXPECT_NE(csr->getOsContext().getContextId(), csr2->getOsContext().getContextId());
            EXPECT_NE(tagAddress, tagAddress2);
        } else {
            EXPECT_EQ(device->secondaryEngines.end(), device->secondaryEngines.find(engineType));
        }
    }

    device.reset(nullptr);

    EXPECT_EQ(0u, memoryManager->secondaryEngines[0].size());
    EXPECT_EQ(0u, memoryManager->allRegisteredEngines[0].size());

    EXPECT_GT(memoryManager->maxOsContextCount, memoryManager->latestContextId);

    executionEnvironment->decRefInternal();
}

HWTEST_F(DeviceTests, givenDebugFlagSetWhenCreatingSecondaryEnginesThenSkipSelectedEngineTypes) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    uint32_t computeEngineBit = 1 << static_cast<uint32_t>(aub_stream::EngineType::ENGINE_CCS);

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);
    debugManager.flags.SecondaryContextEngineTypeMask.set(~computeEngineBit);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->incRefInternal();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

    EXPECT_EQ(device->secondaryEngines.end(), device->secondaryEngines.find(aub_stream::ENGINE_CCS));

    executionEnvironment->decRefInternal();
}

HWTEST_F(DeviceTests, givenHpCopyEngineAndDebugFlagSetWhenCreatingSecondaryEnginesThenSkipSelectedEngineTypes) {
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 0b111;

    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u));
    const auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    auto hpEngine = gfxCoreHelper.getDefaultHpCopyEngine(hwInfo);
    if (hpEngine == aub_stream::EngineType::NUM_ENGINES) {
        GTEST_SKIP();
    }

    uint32_t computeEngineBit = 1 << static_cast<uint32_t>(hpEngine);
    debugManager.flags.SecondaryContextEngineTypeMask.set(~computeEngineBit);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment.release(), 0));

    EXPECT_NE(nullptr, device->getHpCopyEngine());

    EXPECT_EQ(device->secondaryEngines.end(), device->secondaryEngines.find(hpEngine));
}

HWTEST_F(DeviceTests, givenHpCopyEngineAndAggregatedProcessCountWhenCreatingSecondaryEnginesThenContextCountIsDividedByProcessCount) {
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(64);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 0b111;

    // Determine hpEngine type first
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u));
    const auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto hpEngine = gfxCoreHelper.getDefaultHpCopyEngine(hwInfo);
    if (hpEngine == aub_stream::EngineType::NUM_ENGINES) {
        GTEST_SKIP();
    }

    // Test single process scenario first
    {
        auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

        EXPECT_NE(nullptr, device->getHpCopyEngine());

        if (device->secondaryEngines.find(hpEngine) != device->secondaryEngines.end()) {
            auto &secondaryEngines = device->secondaryEngines[hpEngine];
            auto expectedContextCount = gfxCoreHelper.getContextGroupContextsCount();
            // Without process division, should have full context group count (64 contexts total)
            EXPECT_EQ(expectedContextCount, secondaryEngines.engines.size());
        }
    }

    // Test multi-process scenario with process count division
    {
        auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
        auto osInterface = new MockOsInterface();
        auto driverModelMock = std::make_unique<MockDriverModel>();
        osInterface->setDriverModel(std::move(driverModelMock));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);

        const auto numProcesses = 4u;
        osInterface->numberOfProcesses = numProcesses;

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

        EXPECT_NE(nullptr, device->getHpCopyEngine());

        if (device->secondaryEngines.find(hpEngine) != device->secondaryEngines.end()) {
            auto &secondaryEngines = device->secondaryEngines[hpEngine];

            // With process division: max(64/4, 2) = max(16, 2) = 16 contexts total
            const uint32_t expectedContextCount = std::max(gfxCoreHelper.getContextGroupContextsCount() / numProcesses, 2u);
            EXPECT_EQ(expectedContextCount, secondaryEngines.engines.size());
        }
    }
}

TEST_F(DeviceTests, GivenDebuggingEnabledWhenDeviceIsInitializedThenL0DebuggerIsCreated) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
    EXPECT_NE(nullptr, device->getL0Debugger());
}

TEST_F(DeviceTests, GivenDebuggingEnabledAndInvalidStateSaveAreaHeaderWhenDeviceIsInitializedThenFailureIsReturned) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    VariableBackup<char> backupStateSaveAreaHeader{MockSipData::mockSipKernel->mockStateSaveAreaHeader.data(), '\0'};
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
    EXPECT_EQ(nullptr, device);
}

TEST_F(DeviceTests, givenDebuggerRequestedByUserAndNotAvailableWhenDeviceIsInitializedThenDeviceIsNullAndErrorIsPrinted) {
    extern bool forceCreateNullptrDebugger;

    VariableBackup backupForceCreateNullptrDebugger{&forceCreateNullptrDebugger, true};
    DebugManagerStateRestore restorer;

    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);

    NEO::debugManager.flags.PrintDebugMessages.set(1);
    StreamCapture capture;
    capture.captureStderr();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
    auto output = capture.getCapturedStderr();

    EXPECT_EQ(std::string("Debug mode is not enabled in the system.\n"), output);
    EXPECT_EQ(nullptr, device);
}

TEST_F(DeviceTests, givenDebuggerRequestedByUserWhenDeviceWithSubDevicesCreatedThenInitializeDebuggerOncePerRootDevice) {
    extern size_t createDebuggerCallCount;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.set(1);

    createDebuggerCallCount = 0;
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    UltDeviceFactory deviceFactory{1, 4, *executionEnvironment};
    EXPECT_EQ(1u, createDebuggerCallCount);
    EXPECT_NE(nullptr, deviceFactory.rootDevices[0]->getL0Debugger());
}

TEST(DeviceWithoutAILTest, givenNoAILWhenCreateDeviceThenDeviceIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableAIL.set(false);
    MockReleaseHelper mockReleaseHelper;
    auto hwInfo = *defaultHwInfo;
    setupDefaultFeatureTableAndWorkaroundTable(&hwInfo, mockReleaseHelper);
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    EXPECT_NE(nullptr, device.get());
}

HWTEST_F(DeviceTests, givenCopyInternalEngineWhenStopDirectSubmissionForCopyEngineCalledThenStopDirectSubmission) {
    DebugManagerStateRestore dbgRestorer;
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    debugManager.flags.ForceBCSForInternalCopyEngine.set(0);
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = false;

    UltDeviceFactory factory{1, 0};
    factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular});

    auto device = factory.rootDevices[0];
    auto regularCsr = device->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver;
    auto regularUltCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(regularCsr);
    regularUltCsr->callBaseStopDirectSubmission = false;

    device->stopDirectSubmissionForCopyEngine();
    EXPECT_FALSE(regularUltCsr->stopDirectSubmissionCalled);

    factory.rootDevices[0]->createEngine({aub_stream::EngineType::ENGINE_BCS, EngineUsage::internal});
    device->stopDirectSubmissionForCopyEngine();
    EXPECT_FALSE(regularUltCsr->stopDirectSubmissionCalled);

    regularUltCsr->blitterDirectSubmissionAvailable = true;
    device->stopDirectSubmissionForCopyEngine();
    EXPECT_TRUE(regularUltCsr->stopDirectSubmissionCalled);
}

TEST(Device, givenDeviceWhenGettingMicrosecondResolutionThenCorrectValueReturned) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    uint32_t expectedMicrosecondResolution = 123;
    device->microsecondResolution = expectedMicrosecondResolution;
    EXPECT_EQ(device->getMicrosecondResolution(), expectedMicrosecondResolution);
}

TEST(Device, givenDeviceWhenCallingUsmAllocationPoolMethodsThenCorrectValueReturned) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(nullptr, device->getUsmMemAllocPool());
    device->cleanupUsmAllocationPool();

    auto *usmAllocPool = new MockUsmMemAllocPool;
    device->resetUsmAllocationPool(usmAllocPool);
    EXPECT_EQ(usmAllocPool, device->getUsmMemAllocPool());
    usmAllocPool->callBaseCleanup = false;
    EXPECT_EQ(0u, usmAllocPool->cleanupCalled);
    device->cleanupUsmAllocationPool();
    EXPECT_EQ(1u, usmAllocPool->cleanupCalled);

    EXPECT_EQ(nullptr, device->getUsmMemAllocPoolsManager());
    RootDeviceIndicesContainer rootDeviceIndices = {device->getRootDeviceIndex()};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{device->getRootDeviceIndex(), device->getDeviceBitfield()}};
    MockUsmMemAllocPoolsManager *usmAllocPoolManager = new MockUsmMemAllocPoolsManager(InternalMemoryType::deviceUnifiedMemory, rootDeviceIndices, deviceBitfields, device.get());
    device->resetUsmAllocationPoolManager(usmAllocPoolManager);
    EXPECT_EQ(usmAllocPoolManager, device->getUsmMemAllocPoolsManager());
    usmAllocPoolManager->canAddPoolCallBase = true;
    EXPECT_TRUE(usmAllocPoolManager->canAddPool(UsmMemAllocPoolsManager::poolInfos[0]));
}

TEST(GroupDevicesTest, whenMultipleDevicesAreCreatedThenGroupDevicesCreatesVectorPerEachProductFamilySortedOverGpuTypeAndProductFamily) {
    DebugManagerStateRestore restorer;
    const size_t numRootDevices = 5u;

    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    auto executionEnvironment = new ExecutionEnvironment();

    for (auto i = 0u; i < numRootDevices; i++) {
        executionEnvironment->rootDeviceEnvironments.push_back(std::make_unique<MockRootDeviceEnvironment>(*executionEnvironment));
    }
    auto inputDevices = DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_EQ(numRootDevices, inputDevices.size());

    auto lnl0Device = inputDevices[0].get();
    auto bmg0Device = inputDevices[1].get();
    auto lnl1Device = inputDevices[2].get();
    auto lnl2Device = inputDevices[3].get();
    auto ptl0Device = inputDevices[4].get();

    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[1]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_BMG;
    executionEnvironment->rootDeviceEnvironments[1]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    executionEnvironment->rootDeviceEnvironments[2]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment->rootDeviceEnvironments[2]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[3]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment->rootDeviceEnvironments[3]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment->rootDeviceEnvironments[4]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_PTL;
    executionEnvironment->rootDeviceEnvironments[4]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    auto groupedDevices = Device::groupDevices(std::move(inputDevices));

    EXPECT_EQ(3u, groupedDevices.size());
    EXPECT_EQ(1u, groupedDevices[0].size());
    EXPECT_EQ(1u, groupedDevices[1].size());
    EXPECT_EQ(3u, groupedDevices[2].size());

    EXPECT_EQ(lnl0Device, groupedDevices[2][0].get());
    EXPECT_EQ(lnl1Device, groupedDevices[2][1].get());
    EXPECT_EQ(lnl2Device, groupedDevices[2][2].get());
    EXPECT_EQ(ptl0Device, groupedDevices[1][0].get());
    EXPECT_EQ(bmg0Device, groupedDevices[0][0].get());
}

TEST(GroupDevicesTest, givenEmptyDeviceVectorWhenGroupDevicesThenEmptyVectorIsReturned) {
    DeviceVector inputDevices{};
    auto groupedDevices = Device::groupDevices(std::move(inputDevices));

    EXPECT_TRUE(groupedDevices.empty());
}

TEST(GroupDevicesTest, givenNullInputInDeviceVectorWhenGroupDevicesThenEmptyVectorIsReturned) {
    DeviceVector inputDevices{};
    inputDevices.push_back(nullptr);
    inputDevices.push_back(nullptr);
    auto groupedDevices = Device::groupDevices(std::move(inputDevices));

    EXPECT_TRUE(groupedDevices.empty());
}

HWTEST_F(DeviceTests, givenVariousCsrModeWhenDevicePollForCompletionIsCalledThenPollForCompletionIsCalledCorrectlyOnCommandStreamReceivers) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    std::vector<uint32_t> csrCallCounts;
    csrCallCounts.reserve(device->commandStreamReceivers.size());

    for (uint32_t csrIndex = 0; csrIndex < device->commandStreamReceivers.size(); csrIndex++) {
        auto &csr = device->getUltCommandStreamReceiverFromIndex<FamilyType>(csrIndex);
        csrCallCounts.push_back(csr.pollForCompletionCalled);
    }

    device->getUltCommandStreamReceiver<FamilyType>().setType(CommandStreamReceiverType::hardwareWithAub);
    device->pollForCompletion();

    for (uint32_t csrIndex = 0; csrIndex < device->commandStreamReceivers.size(); csrIndex++) {
        auto &csr = device->getUltCommandStreamReceiverFromIndex<FamilyType>(csrIndex);
        EXPECT_EQ(++csrCallCounts[csrIndex], csr.pollForCompletionCalled);
    }

    device->getUltCommandStreamReceiver<FamilyType>().setType(CommandStreamReceiverType::hardware);
    device->pollForCompletion();

    for (uint32_t csrIndex = 0; csrIndex < device->commandStreamReceivers.size(); csrIndex++) {
        auto &csr = device->getUltCommandStreamReceiverFromIndex<FamilyType>(csrIndex);
        EXPECT_EQ(csrCallCounts[csrIndex], csr.pollForCompletionCalled);
    }
}

HWTEST_F(DeviceTests, givenDeviceWithNoEnginesWhenPollForCompletionIsCalledThenEarlyReturnAndDontCrash) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto &allEngines = const_cast<std::vector<NEO::EngineControl> &>(device->getAllEngines());
    allEngines.clear();

    EXPECT_EQ(device->allEngines.size(), 0u);
    EXPECT_NO_THROW(device->pollForCompletion());
}

HWTEST_F(DeviceTests, givenRootDeviceWhenPollForCompletionIsCalledThenPollForCompletionIsCalledOnAllSubDevices) {
    UltDeviceFactory factory{1, 2};
    auto rootDevice = factory.rootDevices[0];
    for (auto subDevice : factory.subDevices) {
        auto *mockSubDevice = static_cast<MockSubDevice *>(subDevice);
        EXPECT_FALSE(mockSubDevice->pollForCompletionCalled);
    }

    rootDevice->getUltCommandStreamReceiver<FamilyType>().setType(CommandStreamReceiverType::hardwareWithAub);
    rootDevice->pollForCompletion();

    for (auto subDevice : factory.subDevices) {
        auto *mockSubDevice = static_cast<MockSubDevice *>(subDevice);
        EXPECT_TRUE(mockSubDevice->pollForCompletionCalled);
    }
}

HWTEST_F(DeviceTests, givenMaskedSubDevicesWhenCallingPollForCompletionOnRootDeviceThenPollForCompletionIsCalledOnlyOnMaskedDevices) {
    constexpr uint32_t numSubDevices = 3;
    constexpr uint32_t numMaskedSubDevices = 2;

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2");

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->parseAffinityMask();
    UltDeviceFactory deviceFactory{1, numSubDevices, *executionEnvironment.release()};
    auto rootDevice = deviceFactory.rootDevices[0];
    EXPECT_NE(nullptr, rootDevice);
    EXPECT_EQ(numMaskedSubDevices, rootDevice->getNumSubDevices());

    for (auto subDevice : rootDevice->getSubDevices()) {
        if (subDevice != nullptr) {
            auto *mockSubDevice = static_cast<MockSubDevice *>(subDevice);
            EXPECT_FALSE(mockSubDevice->pollForCompletionCalled);
        }
    }

    rootDevice->getUltCommandStreamReceiver<FamilyType>().setType(CommandStreamReceiverType::hardwareWithAub);
    rootDevice->pollForCompletion();

    unsigned int callCount = 0;
    for (auto subDevice : rootDevice->getSubDevices()) {
        if (subDevice != nullptr) {
            auto *mockSubDevice = static_cast<MockSubDevice *>(subDevice);
            if (mockSubDevice->pollForCompletionCalled) {
                callCount++;
            }
        }
    }
    EXPECT_EQ(callCount, numMaskedSubDevices);
}

TEST(DeviceCanAccessPeerTest, givenTheSameDeviceThenCanAccessPeerReturnsTrue) {
    UltDeviceFactory deviceFactory{2, 0};
    auto rootDevice0 = deviceFactory.rootDevices[0];

    auto queryPeerAccess = [](Device &peerDevice, Device &device, bool &canAccess) -> bool {
        canAccess = false;
        return false;
    };

    bool canAccess = false;
    bool result = rootDevice0->canAccessPeer(queryPeerAccess, rootDevice0, canAccess);
    EXPECT_TRUE(result);
    EXPECT_TRUE(canAccess);
}

TEST(DeviceCanAccessPeerTest, givenTwoRootDevicesThenCanAccessPeerReturnsValueBasedOnDebugVariable) {
    DebugManagerStateRestore restorer;

    UltDeviceFactory deviceFactory{2, 0};
    auto rootDevice0 = deviceFactory.rootDevices[0];
    auto rootDevice1 = deviceFactory.rootDevices[1];

    {
        uint32_t flagValue = 1;
        bool canAccess = false;
        auto queryPeerAccess = [&flagValue](Device &peerDevice, Device &device, bool &canAccess) -> bool {
            canAccess = !!flagValue;
            return false;
        };
        debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(flagValue);
        bool result = rootDevice0->canAccessPeer(queryPeerAccess, rootDevice1, canAccess);
        EXPECT_TRUE(result);
        EXPECT_TRUE(canAccess);
    }

    {
        uint32_t flagValue = 0;
        bool canAccess = true;
        auto queryPeerAccess = [&flagValue](Device &peerDevice, Device &device, bool &canAccess) -> bool {
            canAccess = !!flagValue;
            return false;
        };
        debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(flagValue);
        bool result = rootDevice0->canAccessPeer(queryPeerAccess, rootDevice1, canAccess);
        EXPECT_TRUE(result);
        EXPECT_FALSE(canAccess);
    }
}

TEST(DeviceCanAccessPeerTest, givenCanAccessPeerCalledTwiceThenCanAccessPeerCachesResultAndReturnsSameValueEachTime) {
    UltDeviceFactory deviceFactory{2, 0};
    auto rootDevice0 = deviceFactory.rootDevices[0];
    auto rootDevice1 = deviceFactory.rootDevices[1];

    uint32_t queryCalled = 0;

    auto queryPeerAccess = [&queryCalled](Device &peerDevice, Device &device, bool &canAccess) -> bool {
        queryCalled++;
        canAccess = true;
        return true;
    };

    bool canAccess = false;

    bool res = rootDevice0->canAccessPeer(queryPeerAccess, rootDevice1, canAccess);
    EXPECT_EQ(1u, queryCalled);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);

    res = rootDevice0->canAccessPeer(queryPeerAccess, rootDevice1, canAccess);
    EXPECT_EQ(1u, queryCalled);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);
}

TEST(DeviceCanAccessPeerTest, givenTwoSubDevicesFromTheSameRootDeviceThenCanAccessPeerReturnsTrue) {
    UltDeviceFactory deviceFactory{1, 2};

    auto rootDevice = deviceFactory.rootDevices[0];
    auto subDevice0 = rootDevice->getSubDevices()[0];
    auto subDevice1 = rootDevice->getSubDevices()[1];

    auto queryPeerAccess = [](Device &peerDevice, Device &device, bool &canAccess) -> bool {
        canAccess = false;
        return false;
    };

    bool canAccess = false;
    bool res = subDevice0->canAccessPeer(queryPeerAccess, subDevice1, canAccess);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);
}

TEST(DevicePeerAccessInitializationTest, givenDeviceListWhenInitializePeerAccessThenQueryOnlyRelevantPeers) {
    UltDeviceFactory deviceFactory{3, 0};
    std::vector<NEO::Device *> rootDevices = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1], deviceFactory.rootDevices[2]};

    auto releaseHelper0 = std::make_unique<MockReleaseHelper>();
    releaseHelper0->shouldQueryPeerAccessResult = false;
    rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper0);

    auto releaseHelper1 = std::make_unique<MockReleaseHelper>();
    releaseHelper1->shouldQueryPeerAccessResult = true;
    rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper1);

    auto releaseHelper2 = std::make_unique<MockReleaseHelper>();
    releaseHelper2->shouldQueryPeerAccessResult = true;
    rootDevices[2]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper2);

    uint32_t queryCalled = 0;
    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
        queryCalled++;
        canAccess = true;
        return true;
    };

    MockDevice::initializePeerAccessForDevices(queryPeerAccess, rootDevices);

    // Check device[0] with none
    // Check device[1] with device[0] and device[2] - 2 calls
    // Check device[2] with device[0] and device[1] (cached) - 1 call
    uint32_t numberOfCalls = 3;
    EXPECT_EQ(numberOfCalls, queryCalled);
}

TEST(DevicePeerAccessInitializationTest, givenSubDevicesWhenInitializePeerAccessThenSkipPeerAccessQuery) {
    UltDeviceFactory deviceFactory{1, 2};
    std::vector<NEO::Device *> subDevices = {deviceFactory.subDevices[0], deviceFactory.subDevices[1]};

    auto releaseHelper0 = std::make_unique<MockReleaseHelper>();
    releaseHelper0->shouldQueryPeerAccessResult = true;
    subDevices[0]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper0);

    auto releaseHelper1 = std::make_unique<MockReleaseHelper>();
    releaseHelper1->shouldQueryPeerAccessResult = true;
    subDevices[1]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper1);

    uint32_t queryCalled = 0;
    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
        queryCalled++;
        canAccess = true;
        return true;
    };

    MockDevice::initializePeerAccessForDevices(queryPeerAccess, subDevices);

    EXPECT_EQ(0u, queryCalled);
}

TEST(DevicePeerAccessInitializationTest, givenDevicesWithPeerAccessCachedWhenInitializePeerAccessForDevicesThenSkipPeerAccessQuery) {
    UltDeviceFactory deviceFactory{2, 0};
    std::vector<NEO::Device *> rootDevices = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};

    auto releaseHelper0 = std::make_unique<MockReleaseHelper>();
    releaseHelper0->shouldQueryPeerAccessResult = true;
    rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper0);

    auto releaseHelper1 = std::make_unique<MockReleaseHelper>();
    releaseHelper1->shouldQueryPeerAccessResult = true;
    rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper1);

    rootDevices[0]->updatePeerAccessCache(rootDevices[1], true);

    uint32_t queryCalled = 0;
    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
        queryCalled++;
        canAccess = false;
        return true;
    };

    MockDevice::initializePeerAccessForDevices(queryPeerAccess, rootDevices);

    EXPECT_EQ(0u, queryCalled);
}

TEST(DevicePeerAccessInitializationTest, givenDevicesWhenInitializePeerAccessForDevicesThenSetsHasAnyPeerAccessAccordingToP2PConnection) {
    // Devices have P2P connection
    {
        UltDeviceFactory deviceFactory{2, 0};
        std::vector<NEO::Device *> rootDevices = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};

        auto releaseHelper0 = std::make_unique<MockReleaseHelper>();
        releaseHelper0->shouldQueryPeerAccessResult = true;
        rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper0);

        auto releaseHelper1 = std::make_unique<MockReleaseHelper>();
        releaseHelper1->shouldQueryPeerAccessResult = true;
        rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper1);

        uint32_t queryCalled = 0;
        auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
            queryCalled++;
            canAccess = true;
            return true;
        };

        MockDevice::initializePeerAccessForDevices(queryPeerAccess, rootDevices);

        EXPECT_EQ(1u, queryCalled);
        ASSERT_TRUE(rootDevices[0]->hasAnyPeerAccess().has_value());
        ASSERT_TRUE(rootDevices[1]->hasAnyPeerAccess().has_value());
        EXPECT_TRUE(rootDevices[0]->hasAnyPeerAccess().value());
        EXPECT_TRUE(rootDevices[1]->hasAnyPeerAccess().value());
    }

    // Devices do not have P2P connection
    {
        UltDeviceFactory deviceFactory{2, 0};
        std::vector<NEO::Device *> rootDevices = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};

        auto releaseHelper0 = std::make_unique<MockReleaseHelper>();
        releaseHelper0->shouldQueryPeerAccessResult = true;
        rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper0);

        auto releaseHelper1 = std::make_unique<MockReleaseHelper>();
        releaseHelper1->shouldQueryPeerAccessResult = true;
        rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper1);

        uint32_t queryCalled = 0;
        auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
            queryCalled++;
            canAccess = false;
            return true;
        };

        MockDevice::initializePeerAccessForDevices(queryPeerAccess, rootDevices);

        EXPECT_EQ(1u, queryCalled);
        ASSERT_TRUE(rootDevices[0]->hasAnyPeerAccess().has_value());
        ASSERT_TRUE(rootDevices[1]->hasAnyPeerAccess().has_value());
        EXPECT_FALSE(rootDevices[0]->hasAnyPeerAccess().value());
        EXPECT_FALSE(rootDevices[1]->hasAnyPeerAccess().value());
    }
}

TEST(DevicePeerAccessInitializationTest, givenDevicesThatDontRequirePeerAccessQueryWhenInitializePeerAccessThenDontSetHasPeerAccess) {
    UltDeviceFactory deviceFactory{2, 0};
    std::vector<NEO::Device *> rootDevices = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};

    auto releaseHelper0 = std::make_unique<MockReleaseHelper>();
    releaseHelper0->shouldQueryPeerAccessResult = false;
    rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper0);

    auto releaseHelper1 = std::make_unique<MockReleaseHelper>();
    releaseHelper1->shouldQueryPeerAccessResult = false;
    rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper = std::move(releaseHelper1);

    uint32_t queryCalled = 0;
    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
        queryCalled++;
        canAccess = true;
        return true;
    };

    MockDevice::initializePeerAccessForDevices(queryPeerAccess, rootDevices);

    EXPECT_EQ(0u, queryCalled);

    EXPECT_FALSE(rootDevices[0]->hasAnyPeerAccess().has_value());
    EXPECT_FALSE(rootDevices[1]->hasAnyPeerAccess().has_value());
}

TEST(DevicePeerAccessInitializationTest, givenDevicesWithoutReleaseHelperWhenInitializePeerAccessCalledThenDontSetHasPeerAccess) {
    UltDeviceFactory deviceFactory{2, 0};
    std::vector<NEO::Device *> rootDevices = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};

    rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper.reset();
    rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper.reset();

    ASSERT_EQ(nullptr, rootDevices[0]->getRootDeviceEnvironmentRef().releaseHelper);
    ASSERT_EQ(nullptr, rootDevices[1]->getRootDeviceEnvironmentRef().releaseHelper);

    uint32_t queryCalled = 0;
    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, bool &canAccess) -> bool {
        queryCalled++;
        canAccess = true;
        return true;
    };

    MockDevice::initializePeerAccessForDevices(queryPeerAccess, rootDevices);

    EXPECT_EQ(0u, queryCalled);

    EXPECT_FALSE(rootDevices[0]->hasAnyPeerAccess().has_value());
    EXPECT_FALSE(rootDevices[1]->hasAnyPeerAccess().has_value());
}

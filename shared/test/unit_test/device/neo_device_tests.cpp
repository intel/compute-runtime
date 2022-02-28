/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(DeviceBlitterTest, whenBlitterOperationsSupportIsDisabledThenNoInternalCopyEngineIsReturned) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = false;

    UltDeviceFactory factory{1, 0};
    EXPECT_EQ(nullptr, factory.rootDevices[0]->getInternalCopyEngine());
}

TEST(DeviceBlitterTest, givenBlitterOperationsDisabledWhenCreatingBlitterEngineThenAbort) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = false;

    UltDeviceFactory factory{1, 0};
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::Cooperative}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::Internal}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::LowPriority}), std::runtime_error);
}

using DeviceTest = Test<DeviceFixture>;

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledAndRtBackedBufferIsNullptrThenMemoryBackedBufferIsCreated) {
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

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledWithMockAllocatorThenRTDispatchGlobalsIsAllocated) {
    pDevice->setRTDispatchGlobalsForceAllocation();
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(3));
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(5));
}

TEST_F(DeviceTest, whenInitializeRayTracingIsCalledMultipleTimesWithMockAllocatorThenInitializeRayTracingIsIdempotent) {
    pDevice->setRTDispatchGlobalsForceAllocation();
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(5));
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(5));
}

TEST_F(DeviceTest, whenGetRTDispatchGlobalsIsCalledBeforeInitializationThenNullPtrIsReturned) {
    EXPECT_EQ(nullptr, pDevice->getRTDispatchGlobals(1));
}

TEST_F(DeviceTest, whenGetRTDispatchGlobalsIsCalledWithZeroSizeAndMockAllocatorThenDispatchGlobalsIsReturned) {
    pDevice->setRTDispatchGlobalsForceAllocation();
    EXPECT_EQ(nullptr, pDevice->getRTDispatchGlobals(0));
    pDevice->initializeRayTracing(5);
    EXPECT_NE(nullptr, pDevice->getRTDispatchGlobals(0));
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

    DebugManager.flags.CreateMultipleSubDevices.set(4);
    pDevice->deviceBitfield = 15;

    DebugManager.flags.EnableWalkerPartition.set(1);
    DebugManager.flags.EnableSharedSystemUsmSupport.set(1);
    pDevice->initializeCaps();
    EXPECT_TRUE(pDevice->getDeviceInfo().maxMemAllocSize == pDevice->getDeviceInfo().globalMemSize);

    DebugManager.flags.EnableWalkerPartition.set(0);
    DebugManager.flags.EnableSharedSystemUsmSupport.set(1);
    pDevice->initializeCaps();
    EXPECT_TRUE(pDevice->getDeviceInfo().maxMemAllocSize <= pDevice->getDeviceInfo().globalMemSize);

    DebugManager.flags.EnableWalkerPartition.set(1);
    DebugManager.flags.EnableSharedSystemUsmSupport.set(0);
    pDevice->initializeCaps();
    EXPECT_TRUE(pDevice->getDeviceInfo().maxMemAllocSize < pDevice->getDeviceInfo().globalMemSize);

    DebugManager.flags.EnableWalkerPartition.set(0);
    DebugManager.flags.EnableSharedSystemUsmSupport.set(0);
    pDevice->initializeCaps();
    EXPECT_TRUE(pDevice->getDeviceInfo().maxMemAllocSize < pDevice->getDeviceInfo().globalMemSize);
}

TEST_F(DeviceGetCapsTest, givenDontForcePreemptionModeDebugVariableWhenCreateDeviceThenSetDefaultHwPreemptionMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForcePreemptionMode.set(-1);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        EXPECT_TRUE(device->getHardwareInfo().capabilityTable.defaultPreemptionMode ==
                    device->getPreemptionMode());
    }
}

TEST_F(DeviceGetCapsTest, givenDebugFlagSetWhenCreatingDeviceInfoThenOverrideProfilingTimerResolution) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.OverrideProfilingTimerResolution.set(123);

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
        DebugManager.flags.ForcePreemptionMode.set((int32_t)forceMode);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

        EXPECT_TRUE(forceMode == device->getPreemptionMode());
    }
}

TEST_F(DeviceGetCapsTest, givenDeviceWithMidThreadPreemptionWhenDeviceIsCreatedThenSipKernelIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        auto builtIns = new MockBuiltins();
        ASSERT_FALSE(MockSipData::called);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::MidThread);

        auto executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0u]->builtins.reset(builtIns);
        auto device = std::unique_ptr<Device>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
        ASSERT_EQ(builtIns, device->getBuiltIns());
        EXPECT_FALSE(MockSipData::called);
    }
}

TEST_F(DeviceGetCapsTest, whenDriverModelHasLimitationForMaxMemoryAllocationSizeThenTakeItIntoAccount) {
    struct MockDriverModel : NEO::DriverModel {
        size_t maxAllocSize;

        MockDriverModel(size_t maxAllocSize) : NEO::DriverModel(NEO::DriverModelType::UNKNOWN), maxAllocSize(maxAllocSize) {}

        void setGmmInputArgs(void *args) override {}
        uint32_t getDeviceHandle() const override { return {}; }
        PhysicalDevicePciBusInfo getPciBusInfo() const override { return {}; }
        bool isGpuHangDetected(NEO::OsContext &osContext) override {
            return false;
        }

        size_t getMaxMemAllocSize() const override {
            return maxAllocSize;
        }
        PhyicalDevicePciSpeedInfo getPciSpeedInfo() const override { return {}; }
    };

    DebugManagerStateRestore dbgRestorer;
    size_t maxAllocSizeTestValue = 512;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModel>(maxAllocSizeTestValue));
    device->initializeCaps();
    const auto &caps = device->getDeviceInfo();
    EXPECT_EQ(maxAllocSizeTestValue, caps.maxMemAllocSize);
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenVmeIsEnabled) {
    DebugSettingsManager<DebugFunctionalityLevel::RegKeys> freshDebugSettingsManager("");
    EXPECT_TRUE(freshDebugSettingsManager.flags.EnableIntelVme.get());
}

TEST(DeviceGetCapsSimpleTest, givenVariousOclVersionsWhenCapsAreCreatedThenDeviceReportsSpirvAsSupportedIl) {
    DebugManagerStateRestore dbgRestorer;
    int32_t oclVersionsToTest[] = {12, 21, 30};
    for (auto oclVersion : oclVersionsToTest) {
        DebugManager.flags.ForceOCLVersion.set(oclVersion);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("SPIR-V_1.2 ", caps.ilVersion);
    }
}

TEST(DeviceGetCapsSimpleTest, givenDebugFlagToSetWorkgroupSizeWhenDeviceIsCreatedThenItUsesThatWorkgroupSize) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.OverrideMaxWorkgroupSize.set(16u);

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
    VariableBackup<bool> OsEnabled64kbPagesBackup(&OSInterface::osEnabled64kbPages);
    class MockMemoryManager : public MemoryManager {
      public:
        MockMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {}
        void addAllocationToHostPtrManager(GraphicsAllocation *memory) override{};
        void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override{};
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override { return nullptr; };
        GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; };
        AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
        void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
        void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override{};
        void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
        uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override {
            return 0;
        };
        uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
        double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
        AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override {
            return {};
        }
        void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
        GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override { return nullptr; };
        GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
        GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override { return nullptr; };

        GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
        GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override { return nullptr; };
        void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override { return nullptr; };
        void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override{};
    };

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto &capabilityTable = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable;
    std::unique_ptr<MemoryManager> memoryManager;

    DebugManager.flags.Enable64kbpages.set(-1);

    capabilityTable.ftr64KBpages = false;
    OSInterface::osEnabled64kbPages = false;
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    capabilityTable.ftr64KBpages = false;
    OSInterface::osEnabled64kbPages = true;
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    capabilityTable.ftr64KBpages = true;
    OSInterface::osEnabled64kbPages = false;
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    capabilityTable.ftr64KBpages = true;
    OSInterface::osEnabled64kbPages = true;
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));

    DebugManager.flags.Enable64kbpages.set(0); // force false
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    DebugManager.flags.Enable64kbpages.set(1); // force true
    memoryManager.reset(new MockMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));
}
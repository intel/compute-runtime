/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include <memory>

using namespace NEO;

typedef Test<ClDeviceFixture> DeviceTest;

TEST_F(DeviceTest, givenDeviceWhenGetProductAbbrevThenReturnsHardwarePrefix) {
    const auto productAbbrev = pDevice->getProductAbbrev();
    const auto hwPrefix = hardwarePrefix[pDevice->getHardwareInfo().platform.eProductFamily];
    EXPECT_EQ(hwPrefix, productAbbrev);
}

TEST_F(DeviceTest, WhenDeviceIsCreatedThenCommandStreamReceiverIsNotNull) {
    EXPECT_NE(nullptr, &pDevice->getGpgpuCommandStreamReceiver());
}

TEST_F(DeviceTest, WhenDeviceIsCreatedThenEnabledClVersionMatchesHardwareInfo) {
    auto version = pClDevice->getEnabledClVersion();
    auto version2 = pDevice->getHardwareInfo().capabilityTable.clVersionSupport;

    EXPECT_EQ(version, version2);
}

TEST_F(DeviceTest, givenDeviceWhenEngineIsCreatedThenSetInitialValueForTag) {
    for (auto &engine : pDevice->allEngines) {
        auto tagAddress = engine.commandStreamReceiver->getTagAddress();
        ASSERT_NE(nullptr, const_cast<TaskCountType *>(tagAddress));
        EXPECT_EQ(initialHardwareTag, *tagAddress);
    }
}

TEST_F(DeviceTest, givenDeviceWhenAskedForSpecificEngineThenReturnIt) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockClDevice mockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0)};
    auto &gfxCoreHelper = mockClDevice.getGfxCoreHelper();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(mockClDevice.getRootDeviceEnvironment());
    for (uint32_t i = 0; i < engines.size(); i++) {
        auto &deviceEngine = mockClDevice.getEngine(engines[i].first, EngineUsage::regular);
        EXPECT_EQ(deviceEngine.osContext->getEngineType(), engines[i].first);
        EXPECT_EQ(deviceEngine.osContext->isLowPriority(), false);
    }

    auto &deviceEngine = mockClDevice.getEngine(hwInfo.capabilityTable.defaultEngineType, EngineUsage::lowPriority);
    EXPECT_EQ(deviceEngine.osContext->getEngineType(), hwInfo.capabilityTable.defaultEngineType);
    EXPECT_EQ(deviceEngine.osContext->isLowPriority(), true);

    EXPECT_THROW(mockClDevice.getEngine(aub_stream::ENGINE_VCS, EngineUsage::regular), std::exception);
}

TEST_F(DeviceTest, givenDebugVariableToAlwaysChooseEngineZeroWhenNotExistingEngineSelectedThenIndexZeroEngineIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideInvalidEngineWithDefault.set(true);
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(pDevice->getRootDeviceEnvironment());
    auto &deviceEngine = pDevice->getEngine(engines[0].first, EngineUsage::regular);
    auto &notExistingEngine = pDevice->getEngine(aub_stream::ENGINE_VCS, EngineUsage::regular);
    EXPECT_EQ(&notExistingEngine, &deviceEngine);
}

TEST_F(DeviceTest, WhenDeviceIsCreatedThenOsTimeIsNotNull) {
    auto pDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    OSTime *osTime = pDevice->getOSTime();
    ASSERT_NE(nullptr, osTime);
}

TEST_F(DeviceTest, GivenDebugVariableForcing32BitAllocationsWhenDeviceIsCreatedThenMemoryManagerHasForce32BitFlagSet) {
    debugManager.flags.Force32bitAddressing.set(true);
    auto pDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    if constexpr (is64bit) {
        EXPECT_TRUE(pDevice->getDeviceInfo().force32BitAddresses);
        EXPECT_TRUE(pDevice->getMemoryManager()->peekForce32BitAllocations());
    } else {
        EXPECT_FALSE(pDevice->getDeviceInfo().force32BitAddresses);
        EXPECT_FALSE(pDevice->getMemoryManager()->peekForce32BitAllocations());
    }
    debugManager.flags.Force32bitAddressing.set(false);
}

TEST_F(DeviceTest, WhenRetainingThenReferenceIsOneAndApiIsUsed) {
    ASSERT_NE(nullptr, pClDevice);

    pClDevice->retainApi();
    pClDevice->retainApi();
    pClDevice->retainApi();
    ASSERT_EQ(1, pClDevice->getReference());

    ASSERT_FALSE(pClDevice->releaseApi().isUnused());
    ASSERT_EQ(1, pClDevice->getReference());
}

TEST_F(DeviceTest, givenNoPciBusInfoThenIsPciBusInfoValidReturnsFalse) {
    PhysicalDevicePciBusInfo invalidPciBusInfoList[] = {
        PhysicalDevicePciBusInfo(0, 1, 2, PhysicalDevicePciBusInfo::invalidValue),
        PhysicalDevicePciBusInfo(0, 1, PhysicalDevicePciBusInfo::invalidValue, 3),
        PhysicalDevicePciBusInfo(0, PhysicalDevicePciBusInfo::invalidValue, 2, 3),
        PhysicalDevicePciBusInfo(PhysicalDevicePciBusInfo::invalidValue, 1, 2, 3)};

    for (auto pciBusInfo : invalidPciBusInfoList) {
        auto driverInfo = new DriverInfoMock();
        driverInfo->setPciBusInfo(pciBusInfo);

        pClDevice->driverInfo.reset(driverInfo);
        pClDevice->initializeCaps();

        EXPECT_FALSE(pClDevice->isPciBusInfoValid());
    }
}

TEST_F(DeviceTest, givenPciBusInfoThenIsPciBusInfoValidReturnsTrue) {
    PhysicalDevicePciBusInfo pciBusInfo(0, 1, 2, 3);

    auto driverInfo = new DriverInfoMock();
    driverInfo->setPciBusInfo(pciBusInfo);

    pClDevice->driverInfo.reset(driverInfo);
    pClDevice->initializeCaps();

    EXPECT_TRUE(pClDevice->isPciBusInfoValid());
}

HWTEST_F(DeviceTest, WhenDeviceIsCreatedThenActualEngineTypeIsSameAsDefault) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType == aub_stream::EngineType::ENGINE_CCS) {
        hwInfo.featureTable.flags.ftrCCSNode = true;
    }

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    auto actualEngineType = device->getDefaultEngine().osContext->getEngineType();
    auto defaultEngineType = device->getHardwareInfo().capabilityTable.defaultEngineType;

    EXPECT_EQ(&device->getDefaultEngine().commandStreamReceiver->getOsContext(), device->getDefaultEngine().osContext);
    EXPECT_EQ(defaultEngineType, actualEngineType);

    int defaultCounter = 0;
    const auto &engines = device->getAllEngines();
    for (const auto &engine : engines) {
        if (engine.osContext->isDefaultContext()) {
            defaultCounter++;
        }
    }
    EXPECT_EQ(defaultCounter, 1);
}

TEST_F(DeviceTest, givenDeviceWithThreadsPerEUConfigsWhenQueryingEuThreadCountsThenConfigsAreReturned) {
    cl_int retVal = CL_SUCCESS;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(NEO::defaultHwInfo.get(), 0));
    const StackVec<uint32_t, 6> configs = {123U, 456U};
    device->sharedDeviceInfo.threadsPerEUConfigs = configs;

    size_t paramRetSize;
    retVal = device->getDeviceInfo(CL_DEVICE_EU_THREAD_COUNTS_INTEL, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(configs.size() * sizeof(cl_uint), paramRetSize);

    auto euThreadCounts = std::make_unique<uint32_t[]>(paramRetSize / sizeof(cl_uint));
    retVal = device->getDeviceInfo(CL_DEVICE_EU_THREAD_COUNTS_INTEL, paramRetSize, euThreadCounts.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(123U, euThreadCounts[0]);
    EXPECT_EQ(456U, euThreadCounts[1]);
}

TEST_F(DeviceTest, givenRootDeviceWithSubDevicesWhenCreatingThenRootDeviceContextIsInitialized) {
    DebugManagerStateRestore restore{};
    debugManager.flags.DeferOsContextInitialization.set(1);

    UltDeviceFactory factory(1, 2);
    MockDevice &device = *factory.rootDevices[0];

    EXPECT_TRUE(device.getDefaultEngine().osContext->isInitialized());
}

HWTEST_F(DeviceTest, givenDeviceWithoutSubDevicesWhenCreatingContextsThenMemoryManagerDefaultContextIsSetCorrectly) {
    UltDeviceFactory factory(1, 1);
    MockDevice &device = *factory.rootDevices[0];
    auto rootDeviceIndex = device.getRootDeviceIndex();
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());

    OsContext *defaultOsContextMemoryManager = memoryManager->getRegisteredEngines(rootDeviceIndex)[memoryManager->defaultEngineIndex[rootDeviceIndex]].osContext;
    OsContext *defaultOsContextRootDevice = device.getDefaultEngine().osContext;
    EXPECT_EQ(defaultOsContextRootDevice, defaultOsContextMemoryManager);
}

HWTEST_F(DeviceTest, givenDeviceWithSubDevicesWhenCreatingContextsThenMemoryManagerDefaultContextIsSetCorrectly) {
    UltDeviceFactory factory(1, 2);
    MockDevice &device = *factory.rootDevices[0];
    auto rootDeviceIndex = device.getRootDeviceIndex();
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());

    OsContext *defaultOsContextMemoryManager = memoryManager->getRegisteredEngines(rootDeviceIndex)[memoryManager->defaultEngineIndex[rootDeviceIndex]].osContext;
    OsContext *defaultOsContextRootDevice = device.getDefaultEngine().osContext;
    EXPECT_EQ(defaultOsContextRootDevice, defaultOsContextMemoryManager);
}

HWTEST_F(DeviceTest, givenMultiDeviceWhenCreatingContextsThenMemoryManagerDefaultContextIsSetCorrectly) {
    UltDeviceFactory factory(3, 2);
    MockDevice &device = *factory.rootDevices[2];
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());

    for (auto &pRootDevice : factory.rootDevices) {
        auto rootDeviceIndex = pRootDevice->getRootDeviceIndex();
        OsContext *defaultOsContextMemoryManager = memoryManager->getRegisteredEngines(rootDeviceIndex)[memoryManager->defaultEngineIndex[rootDeviceIndex]].osContext;
        OsContext *defaultOsContextRootDevice = pRootDevice->getDefaultEngine().osContext;
        EXPECT_EQ(defaultOsContextRootDevice, defaultOsContextMemoryManager);
    }
}

TEST(DeviceCleanup, givenDeviceWhenItIsDestroyedThenFlushBatchedSubmissionsIsCalled) {
    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCommandStreamReceiver>();
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver *csr = static_cast<MockCommandStreamReceiver *>(&mockDevice->getGpgpuCommandStreamReceiver());
    int flushedBatchedSubmissionsCalledCount = 0;
    csr->flushBatchedSubmissionsCallCounter = &flushedBatchedSubmissionsCalledCount;
    mockDevice.reset(nullptr);

    EXPECT_EQ(1, flushedBatchedSubmissionsCalledCount);
}

TEST(DeviceCreation, GiveNonExistingFclWhenCreatingDeviceThenCompilerInterfaceIsNotCreated) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForcePreemptionMode.set(PreemptionMode::Disabled);
    VariableBackup<const char *> frontEndDllName(&Os::frontEndDllName);
    Os::frontEndDllName = "_fake_fcl1_so";

    auto mockDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    ASSERT_NE(nullptr, mockDevice);

    auto compilerInterface = mockDevice->getCompilerInterface();
    ASSERT_EQ(nullptr, compilerInterface);
}

TEST(DeviceCreation, givenDeviceWhenItIsCreatedThenOsContextIsRegisteredInMemoryManager) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    auto memoryManager = device->getMemoryManager();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto numEnginesForDevice = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment()).size();
    if (device->getNumGenericSubDevices() > 1) {
        numEnginesForDevice *= device->getNumGenericSubDevices();
        numEnginesForDevice += device->allEngines.size();

        if (device->getSubDevice(0)->getNumSubDevices() > 0) {
            numEnginesForDevice += device->getNumSubDevices();
        }
    } else if (device->getNumSubDevices() > 0) {
        numEnginesForDevice += device->getNumSubDevices();
    }
    for (const auto &secondaryContexts : device->secondaryEngines) {
        numEnginesForDevice += secondaryContexts.second.engines.size() - 1;
    }

    EXPECT_EQ(numEnginesForDevice, memoryManager->getRegisteredEngines(device->getRootDeviceIndex()).size());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachOsContextHasUniqueId) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < numDevices; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    }
    executionEnvironment->calculateMaxOsContextCount();

    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    MockDevice *devices[] = {device1.get(), device2.get()};
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());

    auto &gfxCoreHelper = device1->getGfxCoreHelper();
    const auto &numGpgpuEngines = static_cast<uint32_t>(gfxCoreHelper.getGpgpuEngineInstances(device1->getRootDeviceEnvironment()).size());

    size_t numExpectedGenericEnginesPerDevice = numGpgpuEngines;
    auto expectedTotalRegisteredEnginesPerRootDevice = numExpectedGenericEnginesPerDevice;

    uint32_t contextId = 0;
    for (uint32_t i = 0; i < numDevices; i++) {
        uint32_t contextWithinRootDevice = 0;
        size_t numSecondaryEngines = memoryManager->secondaryEngines[i].size();

        auto &registeredEngines = executionEnvironment->memoryManager->getRegisteredEngines(i);

        EXPECT_EQ(expectedTotalRegisteredEnginesPerRootDevice + numSecondaryEngines, registeredEngines.size());
        auto device = devices[i];

        for (uint32_t j = 0; j < numExpectedGenericEnginesPerDevice; j++) {
            auto &engine = device->getEngine(j);

            EXPECT_EQ(contextId, engine.osContext->getContextId());
            EXPECT_EQ(1u, engine.osContext->getDeviceBitfield().to_ulong());

            EXPECT_EQ(registeredEngines[contextWithinRootDevice].commandStreamReceiver,
                      engine.commandStreamReceiver);

            contextId++;
            contextWithinRootDevice++;
        }
        for (uint32_t j = 0; j < numExpectedGenericEnginesPerDevice; j++) {
            auto &engine = device->getEngine(j);
            if (engine.osContext->getIsPrimaryEngine()) {
                for (size_t secondaryEngineId = 1; secondaryEngineId < device->secondaryEngines[engine.getEngineType()].engines.size(); secondaryEngineId++) {

                    EXPECT_EQ(contextId, device->secondaryEngines[engine.getEngineType()].engines[secondaryEngineId].osContext->getContextId());
                    contextId++;
                }
            }
        }
    }
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachDeviceHasSeparateDeviceIndex) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());

        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    executionEnvironment->calculateMaxOsContextCount();

    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    EXPECT_EQ(0u, device->getRootDeviceIndex());
    EXPECT_EQ(1u, device2->getRootDeviceIndex());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachDeviceHasSeparateCommandStreamReceiver) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    }
    executionEnvironment->calculateMaxOsContextCount();

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    const auto &numGpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0]).size();
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    EXPECT_EQ(numGpgpuEngines, device1->commandStreamReceivers.size());
    EXPECT_EQ(numGpgpuEngines, device2->commandStreamReceivers.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(numGpgpuEngines); i++) {
        EXPECT_NE(device2->allEngines[i].commandStreamReceiver, device1->allEngines[i].commandStreamReceiver);
    }
}

HWTEST_F(DeviceTest, givenDeviceWhenAskingForDefaultEngineThenReturnValidValue) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
    gfxCoreHelper.adjustDefaultEngineType(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo(), productHelper, executionEnvironment->rootDeviceEnvironments[0]->ailConfiguration.get());

    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0));
    auto osContext = device->getDefaultEngine().osContext;

    auto defaultEngineType = device->getHardwareInfo().capabilityTable.defaultEngineType;
    auto engineType = osContext->getEngineType();
    EXPECT_EQ(defaultEngineType, engineType);
    EXPECT_FALSE(osContext->isLowPriority());
}

HWTEST_F(DeviceTest, givenDebugFlagWhenCreatingRootDeviceWithSubDevicesThenWorkPartitionAllocationIsCreatedForRootDevice) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableImplicitScaling.set(1);
    {
        UltDeviceFactory deviceFactory{1, 2};
        EXPECT_NE(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_TRUE(deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
    }
    {
        debugManager.flags.EnableStaticPartitioning.set(0);
        UltDeviceFactory deviceFactory{1, 2};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_FALSE(deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
    }
    {
        debugManager.flags.EnableStaticPartitioning.set(1);
        UltDeviceFactory deviceFactory{1, 2};
        EXPECT_NE(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_TRUE(deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
    }
}

HWTEST_F(DeviceTest, givenDebugFlagWhenCreatingRootDeviceWithoutSubDevicesThenWorkPartitionAllocationIsNotCreated) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableImplicitScaling.set(1);
    {
        UltDeviceFactory deviceFactory{1, 1};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
    }
    {
        debugManager.flags.EnableStaticPartitioning.set(0);
        UltDeviceFactory deviceFactory{1, 1};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
    }
    {
        debugManager.flags.EnableStaticPartitioning.set(1);
        UltDeviceFactory deviceFactory{1, 1};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
    }
}

TEST(DeviceCreation, givenDeviceWhenCheckingGpgpuEnginesCountThenNumberGreaterThanZeroIsReturned) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_GT(gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment()).size(), 0u);
}

TEST(DeviceCreation, givenDeviceWhenCheckingParentDeviceThenCorrectValueIsReturned) {
    UltDeviceFactory deviceFactory{2, 2};

    EXPECT_EQ(deviceFactory.rootDevices[0], deviceFactory.rootDevices[0]->getRootDevice());
    EXPECT_EQ(deviceFactory.rootDevices[0], deviceFactory.subDevices[0]->getRootDevice());
    EXPECT_EQ(deviceFactory.rootDevices[0], deviceFactory.subDevices[1]->getRootDevice());

    EXPECT_EQ(deviceFactory.rootDevices[1], deviceFactory.rootDevices[1]->getRootDevice());
    EXPECT_EQ(deviceFactory.rootDevices[1], deviceFactory.subDevices[2]->getRootDevice());
    EXPECT_EQ(deviceFactory.rootDevices[1], deviceFactory.subDevices[3]->getRootDevice());
}

TEST(DeviceCreation, givenRootDeviceWithSubDevicesWhenCheckingEngineGroupsThenItHasOneNonEmptyGroup) {
    UltDeviceFactory deviceFactory{1, 2};
    EXPECT_EQ(1u, deviceFactory.rootDevices[0]->getRegularEngineGroups().size());
}

TEST(DeviceCreation, whenCheckingEngineGroupsThenGroupsAreUnique) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    for (auto blitterOperationsSupported : ::testing::Bool()) {
        defaultHwInfo->capabilityTable.blitterOperationsSupported = blitterOperationsSupported;
        for (auto ftrRcsNode : ::testing::Bool()) {
            defaultHwInfo->featureTable.flags.ftrRcsNode = ftrRcsNode;
            for (auto ftrCCSNode : ::testing::Bool()) {
                defaultHwInfo->featureTable.flags.ftrCCSNode = ftrCCSNode;

                UltDeviceFactory deviceFactory{1, 0};
                std::set<EngineGroupType> uniqueEngineGroupTypes;
                for (auto &engineGroup : deviceFactory.rootDevices[0]->getRegularEngineGroups()) {
                    uniqueEngineGroupTypes.insert(engineGroup.engineGroupType);
                }
                EXPECT_EQ(uniqueEngineGroupTypes.size(), deviceFactory.rootDevices[0]->getRegularEngineGroups().size());
            }
        }
    }
}

using DeviceHwTest = ::testing::Test;

HWTEST_F(DeviceHwTest, givenGfxCoreHelperInputWhenInitializingCsrThenCreatePageTableManagerIfNeeded) {
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;

    MockExecutionEnvironment executionEnvironment(&localHwInfo, true, 3u);
    executionEnvironment.incRefInternal();

    std::unique_ptr<MockDevice> device;
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&localHwInfo, &executionEnvironment, 0));
    auto &csr0 = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr0.createPageTableManagerCalled);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo->capabilityTable.ftrRenderCompressedImages = false;
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&localHwInfo, &executionEnvironment, 1));
    auto &csr1 = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(csr1.needsPageTableManager(), csr1.createPageTableManagerCalled);

    hwInfo = executionEnvironment.rootDeviceEnvironments[2]->getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo->capabilityTable.ftrRenderCompressedImages = true;
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&localHwInfo, &executionEnvironment, 2));
    auto &csr2 = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(csr2.needsPageTableManager(), csr2.createPageTableManagerCalled);
}

HWTEST_F(DeviceHwTest, givenDeviceCreationWhenCsrFailsToCreateGlobalSyncAllocationThenReturnNull) {
    class MockUltCsrThatFailsToCreateGlobalFenceAllocation : public UltCommandStreamReceiver<FamilyType> {
      public:
        MockUltCsrThatFailsToCreateGlobalFenceAllocation(ExecutionEnvironment &executionEnvironment,
                                                         const DeviceBitfield deviceBitfield)
            : UltCommandStreamReceiver<FamilyType>(executionEnvironment, 0, deviceBitfield) {}
        bool createGlobalFenceAllocation() override {
            return false;
        }
    };
    class MockDeviceThatFailsToCreateGlobalFenceAllocation : public MockDevice {
      public:
        MockDeviceThatFailsToCreateGlobalFenceAllocation(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
            : MockDevice(executionEnvironment, deviceIndex) {}
        std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override {
            return std::make_unique<MockUltCsrThatFailsToCreateGlobalFenceAllocation>(*executionEnvironment, getDeviceBitfield());
        }
    };

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    auto mockDevice(MockDevice::create<MockDeviceThatFailsToCreateGlobalFenceAllocation>(executionEnvironment, 0));
    EXPECT_EQ(nullptr, mockDevice);
}

HWTEST_F(DeviceHwTest, givenBothCcsAndRcsEnginesInDeviceWhenGettingEngineGroupsThenReturnInCorrectOrder) {
    struct MyGfxCoreHelper : GfxCoreHelperHw<FamilyType> {
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            if (engineType == aub_stream::ENGINE_RCS) {
                return EngineGroupType::renderCompute;
            }
            if (EngineHelpers::isCcs(engineType)) {
                return EngineGroupType::compute;
            }
            UNRECOVERABLE_IF(true);
        }
    };
    MockDevice device{};

    RAIIGfxCoreHelperFactory<MyGfxCoreHelper> overrideGfxCoreHelper{
        *device.executionEnvironment->rootDeviceEnvironments[0]};

    MockOsContext rcsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}));
    EngineControl rcsEngine{nullptr, &rcsContext};

    MockOsContext ccsContext(1, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));
    EngineControl ccsEngine{nullptr, &ccsContext};

    ASSERT_EQ(0u, device.getRegularEngineGroups().size());
    device.addEngineToEngineGroup(ccsEngine);
    device.addEngineToEngineGroup(rcsEngine);
    auto &engineGroups = device.getRegularEngineGroups();
    EXPECT_EQ(1u, engineGroups[0].engines.size());
    EXPECT_EQ(EngineGroupType::compute, engineGroups[0].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, engineGroups[0].engines[0].getEngineType());
    EXPECT_EQ(1u, engineGroups[1].engines.size());
    EXPECT_EQ(EngineGroupType::renderCompute, engineGroups[1].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_RCS, engineGroups[1].engines[0].getEngineType());

    device.getRegularEngineGroups().clear();
    device.addEngineToEngineGroup(rcsEngine);
    device.addEngineToEngineGroup(ccsEngine);
    engineGroups = device.getRegularEngineGroups();
    EXPECT_EQ(1u, engineGroups[0].engines.size());
    EXPECT_EQ(EngineGroupType::renderCompute, engineGroups[0].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_RCS, engineGroups[0].engines[0].getEngineType());
    EXPECT_EQ(1u, engineGroups[1].engines.size());
    EXPECT_EQ(EngineGroupType::compute, engineGroups[1].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, engineGroups[1].engines[0].getEngineType());
}

TEST(DeviceGetEngineTest, givenHwCsrModeWhenGetEngineThenDedicatedForInternalUsageEngineIsReturned) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto &internalEngine = device->getInternalEngine();
    auto &defaultEngine = device->getDefaultEngine();
    EXPECT_NE(defaultEngine.commandStreamReceiver, internalEngine.commandStreamReceiver);
}

TEST(DeviceGetEngineTest, whenCreateDeviceThenInternalEngineHasDefaultType) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto internalEngineType = device->getInternalEngine().osContext->getEngineType();
    auto defaultEngineType = getChosenEngineType(device->getHardwareInfo());
    EXPECT_EQ(defaultEngineType, internalEngineType);
}

TEST(DeviceGetEngineTest, givenCreatedDeviceWhenRetrievingDefaultEngineThenOsContextHasDefaultFieldSet) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto &defaultEngine = device->getDefaultEngine();
    EXPECT_TRUE(defaultEngine.osContext->isDefaultContext());
}

TEST(DeviceGetEngineTest, givenVariousIndicesWhenGettingEngineGroupIndexFromEngineGroupTypeThenReturnCorrectResults) {
    const auto nonEmptyEngineGroup = std::vector<EngineControl>{EngineControl{nullptr, nullptr}};

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto &engineGroups = device->getRegularEngineGroups();
    engineGroups.resize(3);
    engineGroups[0].engineGroupType = static_cast<EngineGroupType>(4);
    engineGroups[1].engineGroupType = static_cast<EngineGroupType>(3);
    engineGroups[2].engineGroupType = static_cast<EngineGroupType>(2);

    EXPECT_EQ(0u, device->getEngineGroupIndexFromEngineGroupType(static_cast<EngineGroupType>(4u)));
    EXPECT_EQ(1u, device->getEngineGroupIndexFromEngineGroupType(static_cast<EngineGroupType>(3u)));
    EXPECT_EQ(2u, device->getEngineGroupIndexFromEngineGroupType(static_cast<EngineGroupType>(2u)));
    EXPECT_ANY_THROW(device->getEngineGroupIndexFromEngineGroupType(static_cast<EngineGroupType>(1u)));
    EXPECT_ANY_THROW(device->getEngineGroupIndexFromEngineGroupType(static_cast<EngineGroupType>(0u)));
}

TEST(DeviceGetEngineTest, givenDeferredContextInitializationEnabledWhenCreatingEnginesThenInitializeOnlyOsContextsWhichRequireIt) {
    DebugManagerStateRestore restore{};
    debugManager.flags.DeferOsContextInitialization.set(1);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    const auto defaultEngineType = getChosenEngineType(device->getHardwareInfo());
    EXPECT_NE(0u, device->getAllEngines().size());
    for (const EngineControl &engine : device->getAllEngines()) {
        OsContext *osContext = engine.osContext;
        const bool isDefaultEngine = defaultEngineType == osContext->getEngineType() && osContext->isRegular();
        const bool shouldBeInitialized = osContext->isImmediateContextInitializationEnabled(isDefaultEngine);
        EXPECT_EQ(shouldBeInitialized, osContext->isInitialized());
    }
}

TEST(DeviceGetEngineTest, givenDeferredContextInitializationDisabledWhenCreatingEnginesThenInitializeAllOsContexts) {
    DebugManagerStateRestore restore{};
    debugManager.flags.DeferOsContextInitialization.set(0);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_NE(0u, device->getAllEngines().size());
    for (const EngineControl &engine : device->getAllEngines()) {
        EXPECT_TRUE(engine.osContext->isInitialized());
    }
}

TEST(DeviceGetEngineTest, givenNonHwCsrModeWhenGetEngineThenDefaultEngineIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::aub));

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto &internalEngine = device->getInternalEngine();
    auto &defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(defaultEngine.commandStreamReceiver, internalEngine.commandStreamReceiver);
}

using QueueFamiliesTests = ::testing::Test;

HWTEST_F(QueueFamiliesTests, whenGettingQueueFamilyCapabilitiesAllThenReturnCorrectValue) {
    const cl_command_queue_capabilities_intel expectedProperties = CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL |
                                                                   CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL |
                                                                   CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL |
                                                                   CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL |
                                                                   CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL |
                                                                   CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL |
                                                                   CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL |
                                                                   CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL |
                                                                   CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL |
                                                                   CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL |
                                                                   CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL |
                                                                   CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL |
                                                                   CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL |
                                                                   CL_QUEUE_CAPABILITY_MARKER_INTEL |
                                                                   CL_QUEUE_CAPABILITY_BARRIER_INTEL |
                                                                   CL_QUEUE_CAPABILITY_KERNEL_INTEL;
    EXPECT_EQ(expectedProperties, MockClDevice::getQueueFamilyCapabilitiesAll());
}

HWTEST_F(QueueFamiliesTests, givenComputeQueueWhenGettingQueueFamilyCapabilitiesThenReturnDefaultCapabilities) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(static_cast<uint64_t>(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL), device->getQueueFamilyCapabilities(NEO::EngineGroupType::compute));
    EXPECT_EQ(static_cast<uint64_t>(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL), device->getQueueFamilyCapabilities(NEO::EngineGroupType::renderCompute));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, QueueFamiliesTests, givenCopyQueueWhenGettingQueueFamilyCapabilitiesThenDoNotReturnUnsupportedOperations) {
    const cl_command_queue_capabilities_intel capabilitiesNotSupportedOnBlitter = CL_QUEUE_CAPABILITY_KERNEL_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    const cl_command_queue_capabilities_intel expectedBlitterCapabilities = setBits(MockClDevice::getQueueFamilyCapabilitiesAll(), false, capabilitiesNotSupportedOnBlitter);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(expectedBlitterCapabilities, device->getQueueFamilyCapabilities(NEO::EngineGroupType::copy));
}

TEST(ClDeviceHelperTest, givenNonZeroNumberOfTilesWhenPrepareDeviceEnvironmentsCountCalledThenReturnCorrectValue) {
    DebugManagerStateRestore stateRestore;
    FeatureTable skuTable;
    WorkaroundTable waTable = {};
    RuntimeCapabilityTable capTable = {};
    GT_SYSTEM_INFO sysInfo = {};
    sysInfo.MultiTileArchInfo.IsValid = true;
    sysInfo.MultiTileArchInfo.TileCount = 3;
    PLATFORM platform = {};
    HardwareInfo hwInfo{&platform, &skuTable, &waTable, &sysInfo, capTable};
    debugManager.flags.CreateMultipleSubDevices.set(0);

    uint32_t devicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 3u);
}

TEST(ClDeviceHelperTest, givenZeroNumberOfTilesWhenPrepareDeviceEnvironmentsCountCalledThenReturnCorrectValue) {
    DebugManagerStateRestore stateRestore;
    FeatureTable skuTable;
    WorkaroundTable waTable = {};
    RuntimeCapabilityTable capTable = {};
    GT_SYSTEM_INFO sysInfo = {};
    sysInfo.MultiTileArchInfo.IsValid = true;
    sysInfo.MultiTileArchInfo.TileCount = 0;
    PLATFORM platform = {};
    HardwareInfo hwInfo{&platform, &skuTable, &waTable, &sysInfo, capTable};
    debugManager.flags.CreateMultipleSubDevices.set(0);

    uint32_t devicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 1u);
}

using MultipleDeviceTest = ::testing::TestWithParam<uint32_t>;

TEST_P(MultipleDeviceTest, givenMultipleDevicesWhenGetNumTilesThenReturnNumberOfDevices) {
    DebugManagerStateRestore debugStateRestorer;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    ultHwConfig.forceOsAgnosticMemoryManager = false;
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    auto numDevices = GetParam();
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    debugManager.flags.DeferOsContextInitialization.set(0);
    initPlatform();
    auto device = platform()->getClDevice(0);
    cl_uint numTiles;
    size_t paramRetSize;

    EXPECT_EQ(CL_SUCCESS, device->getDeviceInfo(CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(cl_uint), &numTiles, &paramRetSize));
    cl_uint expectedNumTiles = (numDevices > 1 ? numDevices : 0);
    EXPECT_EQ(expectedNumTiles, numTiles);
    EXPECT_EQ(sizeof(cl_uint), paramRetSize);
    platformsImpl->clear();
}

INSTANTIATE_TEST_SUITE_P(
    MultiDeviceTests,
    MultipleDeviceTest,
    testing::Range<uint32_t>(1u, 5u));

using ClDeviceHelperTests = ::testing::Test;

TEST(DeviceEngineTest, givenDebugVariableOverrideEngineTypeWhenDeviceIsCreatedThenUseDebugVariable) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto actualEngine = defaultHwInfo->capabilityTable.defaultEngineType;
    auto expectedEngine = actualEngine == aub_stream::ENGINE_RCS ? aub_stream::ENGINE_CCS
                                                                 : aub_stream::ENGINE_RCS;

    auto supportedGpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
    if (supportedGpgpuEngines[0].first != expectedEngine) {
        return;
    }
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(expectedEngine));

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(expectedEngine, device->getDefaultEngine().osContext->getEngineType());
}

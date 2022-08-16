/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_hw_helper.h"
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
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
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
        ASSERT_NE(nullptr, const_cast<uint32_t *>(tagAddress));
        EXPECT_EQ(initialHardwareTag, *tagAddress);
    }
}

TEST_F(DeviceTest, givenDeviceWhenAskedForSpecificEngineThenReturnIt) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockClDevice mockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0)};

    auto &engines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);
    for (uint32_t i = 0; i < engines.size(); i++) {
        auto &deviceEngine = mockClDevice.getEngine(engines[i].first, EngineUsage::Regular);
        EXPECT_EQ(deviceEngine.osContext->getEngineType(), engines[i].first);
        EXPECT_EQ(deviceEngine.osContext->isLowPriority(), false);
    }

    auto &deviceEngine = mockClDevice.getEngine(hwInfo.capabilityTable.defaultEngineType, EngineUsage::LowPriority);
    EXPECT_EQ(deviceEngine.osContext->getEngineType(), hwInfo.capabilityTable.defaultEngineType);
    EXPECT_EQ(deviceEngine.osContext->isLowPriority(), true);

    EXPECT_THROW(mockClDevice.getEngine(aub_stream::ENGINE_VCS, EngineUsage::Regular), std::exception);
}

TEST_F(DeviceTest, givenDebugVariableToAlwaysChooseEngineZeroWhenNotExistingEngineSelectedThenIndexZeroEngineIsReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideInvalidEngineWithDefault.set(true);
    auto &engines = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo);
    auto &deviceEngine = pDevice->getEngine(engines[0].first, EngineUsage::Regular);
    auto &notExistingEngine = pDevice->getEngine(aub_stream::ENGINE_VCS, EngineUsage::Regular);
    EXPECT_EQ(&notExistingEngine, &deviceEngine);
}

TEST_F(DeviceTest, WhenDeviceIsCreatedThenOsTimeIsNotNull) {
    auto pDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    OSTime *osTime = pDevice->getOSTime();
    ASSERT_NE(nullptr, osTime);
}

TEST_F(DeviceTest, GivenDebugVariableForcing32BitAllocationsWhenDeviceIsCreatedThenMemoryManagerHasForce32BitFlagSet) {
    DebugManager.flags.Force32bitAddressing.set(true);
    auto pDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    if constexpr (is64bit) {
        EXPECT_TRUE(pDevice->getDeviceInfo().force32BitAddressess);
        EXPECT_TRUE(pDevice->getMemoryManager()->peekForce32BitAllocations());
    } else {
        EXPECT_FALSE(pDevice->getDeviceInfo().force32BitAddressess);
        EXPECT_FALSE(pDevice->getMemoryManager()->peekForce32BitAllocations());
    }
    DebugManager.flags.Force32bitAddressing.set(false);
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

HWTEST_F(DeviceTest, givenNoHwCsrTypeAndModifiedDefaultEngineIndexWhenIsSimulationIsCalledThenTrueIsReturned) {
    EXPECT_FALSE(pDevice->isSimulation());
    auto csr = TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, 0, 1);
    pDevice->defaultEngineIndex = 1;
    pDevice->resetCommandStreamReceiver(csr);

    EXPECT_TRUE(pDevice->isSimulation());

    std::array<CommandStreamReceiverType, 3> exptectedEngineTypes = {CommandStreamReceiverType::CSR_HW,
                                                                     CommandStreamReceiverType::CSR_TBX,
                                                                     CommandStreamReceiverType::CSR_HW};

    for (uint32_t i = 0u; i < 3u; ++i) {
        auto engineType = pDevice->allEngines[i].commandStreamReceiver->getType();
        EXPECT_EQ(exptectedEngineTypes[i], engineType);
    }
}

TEST_F(DeviceTest, givenRootDeviceWithSubDevicesWhenCreatingThenRootDeviceContextIsInitialized) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.DeferOsContextInitialization.set(1);

    UltDeviceFactory factory(1, 2);
    MockDevice &device = *factory.rootDevices[0];

    EXPECT_TRUE(device.getDefaultEngine().osContext->isInitialized());
}

HWTEST_F(DeviceTest, givenDeviceWithoutSubDevicesWhenCreatingContextsThenMemoryManagerDefaultContextIsSetCorrectly) {
    UltDeviceFactory factory(1, 1);
    MockDevice &device = *factory.rootDevices[0];
    auto rootDeviceIndex = device.getRootDeviceIndex();
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());

    OsContext *defaultOsContextMemoryManager = memoryManager->registeredEngines[memoryManager->defaultEngineIndex[rootDeviceIndex]].osContext;
    OsContext *defaultOsContextRootDevice = device.getDefaultEngine().osContext;
    EXPECT_EQ(defaultOsContextRootDevice, defaultOsContextMemoryManager);
}

HWTEST_F(DeviceTest, givenDeviceWithSubDevicesWhenCreatingContextsThenMemoryManagerDefaultContextIsSetCorrectly) {
    UltDeviceFactory factory(1, 2);
    MockDevice &device = *factory.rootDevices[0];
    auto rootDeviceIndex = device.getRootDeviceIndex();
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());

    OsContext *defaultOsContextMemoryManager = memoryManager->registeredEngines[memoryManager->defaultEngineIndex[rootDeviceIndex]].osContext;
    OsContext *defaultOsContextRootDevice = device.getDefaultEngine().osContext;
    EXPECT_EQ(defaultOsContextRootDevice, defaultOsContextMemoryManager);
}

HWTEST_F(DeviceTest, givenMultiDeviceWhenCreatingContextsThenMemoryManagerDefaultContextIsSetCorrectly) {
    UltDeviceFactory factory(3, 2);
    MockDevice &device = *factory.rootDevices[2];
    MockMemoryManager *memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());

    for (auto &pRootDevice : factory.rootDevices) {
        OsContext *defaultOsContextMemoryManager = memoryManager->registeredEngines[memoryManager->defaultEngineIndex[pRootDevice->getRootDeviceIndex()]].osContext;
        OsContext *defaultOsContextRootDevice = pRootDevice->getDefaultEngine().osContext;
        EXPECT_EQ(defaultOsContextRootDevice, defaultOsContextMemoryManager);
    }
}

TEST(DeviceCleanup, givenDeviceWhenItIsDestroyedThenFlushBatchedSubmissionsIsCalled) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver *csr = new MockCommandStreamReceiver(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    mockDevice->resetCommandStreamReceiver(csr);
    int flushedBatchedSubmissionsCalledCount = 0;
    csr->flushBatchedSubmissionsCallCounter = &flushedBatchedSubmissionsCalledCount;
    mockDevice.reset(nullptr);

    EXPECT_EQ(1, flushedBatchedSubmissionsCalledCount);
}

TEST(DeviceCreation, givenSelectedAubCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsTrue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    auto mockDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_TRUE(mockDevice->isSimulation());
}

TEST(DeviceCreation, givenSelectedTbxCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsTrue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_TBX);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_TRUE(device->isSimulation());
}

TEST(DeviceCreation, givenSelectedTbxWithAubCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsTrue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_TBX_WITH_AUB);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_TRUE(device->isSimulation());
}

TEST(DeviceCreation, givenHwWithAubCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsFalse) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_HW_WITH_AUB);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_FALSE(device->isSimulation());
}

TEST(DeviceCreation, givenDefaultHwCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsFalse) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_FALSE(device->isSimulation());
}

TEST(DeviceCreation, givenDeviceWhenItIsCreatedThenOsContextIsRegistredInMemoryManager) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    auto memoryManager = device->getMemoryManager();
    auto numEnginesForDevice = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo).size();
    if (device->getNumGenericSubDevices() > 1) {
        numEnginesForDevice *= device->getNumGenericSubDevices();
        numEnginesForDevice += device->allEngines.size();

        if (device->getSubDevice(0)->getNumSubDevices() > 0) {
            numEnginesForDevice += device->getNumSubDevices();
        }
    } else if (device->getNumSubDevices() > 0) {
        numEnginesForDevice += device->getNumSubDevices();
    }
    EXPECT_EQ(numEnginesForDevice, memoryManager->getRegisteredEnginesCount());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachOsContextHasUniqueId) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < numDevices; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    }

    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    MockDevice *devices[] = {device1.get(), device2.get()};

    auto &registeredEngines = executionEnvironment->memoryManager->getRegisteredEngines();

    auto &hwInfo = device1->getHardwareInfo();
    const auto &numGpgpuEngines = static_cast<uint32_t>(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo).size());

    size_t numExpectedGenericEnginesPerDevice = numGpgpuEngines;
    size_t numExpectedEngineInstancedEnginesPerDevice = 0;
    if (device1->getNumSubDevices() > 0) {
        numExpectedEngineInstancedEnginesPerDevice = device1->getNumSubDevices();
    }

    auto expectedTotalRegisteredEngines = (numExpectedGenericEnginesPerDevice + numExpectedEngineInstancedEnginesPerDevice) * numDevices;

    EXPECT_EQ(expectedTotalRegisteredEngines, registeredEngines.size());

    uint32_t contextId = 0;

    for (uint32_t i = 0; i < numDevices; i++) {
        auto device = devices[i];

        for (uint32_t j = 0; j < numExpectedEngineInstancedEnginesPerDevice; j++) {
            auto subDevice = device->getSubDevice(j);
            auto &engine = subDevice->getEngine(0);
            EXPECT_EQ(contextId, engine.osContext->getContextId());
            EXPECT_EQ(1u, engine.osContext->getDeviceBitfield().to_ulong());

            EXPECT_EQ(registeredEngines[contextId].commandStreamReceiver,
                      engine.commandStreamReceiver);

            contextId++;
        }

        for (uint32_t j = 0; j < numExpectedGenericEnginesPerDevice; j++) {
            auto &engine = device->getEngine(j);

            EXPECT_EQ(contextId, engine.osContext->getContextId());
            EXPECT_EQ(1u, engine.osContext->getDeviceBitfield().to_ulong());

            EXPECT_EQ(registeredEngines[contextId].commandStreamReceiver,
                      engine.commandStreamReceiver);

            contextId++;
        }
    }

    EXPECT_EQ(expectedTotalRegisteredEngines, executionEnvironment->memoryManager->getRegisteredEnginesCount());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachDeviceHasSeperateDeviceIndex) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    EXPECT_EQ(0u, device->getRootDeviceIndex());
    EXPECT_EQ(1u, device2->getRootDeviceIndex());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachDeviceHasSeperateCommandStreamReceiver) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    }
    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    const auto &numGpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo).size();
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
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    hwHelper.adjustDefaultEngineType(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo());

    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0));
    auto osContext = device->getDefaultEngine().osContext;

    EXPECT_EQ(device->getHardwareInfo().capabilityTable.defaultEngineType, osContext->getEngineType());
    EXPECT_FALSE(osContext->isLowPriority());
}

HWTEST_F(DeviceTest, givenDebugFlagWhenCreatingRootDeviceWithSubDevicesThenWorkPartitionAllocationIsCreatedForRootDevice) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableImplicitScaling.set(1);
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
        DebugManager.flags.EnableStaticPartitioning.set(0);
        UltDeviceFactory deviceFactory{1, 2};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_EQ(nullptr, deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
        EXPECT_FALSE(deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[0]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
        EXPECT_FALSE(deviceFactory.subDevices[1]->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled());
    }
    {
        DebugManager.flags.EnableStaticPartitioning.set(1);
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
    DebugManager.flags.EnableImplicitScaling.set(1);
    {
        UltDeviceFactory deviceFactory{1, 1};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
    }
    {
        DebugManager.flags.EnableStaticPartitioning.set(0);
        UltDeviceFactory deviceFactory{1, 1};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
    }
    {
        DebugManager.flags.EnableStaticPartitioning.set(1);
        UltDeviceFactory deviceFactory{1, 1};
        EXPECT_EQ(nullptr, deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation());
    }
}

TEST(DeviceCreation, givenFtrSimulationModeFlagTrueWhenNoOtherSimulationFlagsArePresentThenIsSimulationReturnsTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrSimulationMode = true;

    bool simulationFromDeviceId = hwInfo.capabilityTable.isSimulation(hwInfo.platform.usDeviceID);
    EXPECT_FALSE(simulationFromDeviceId);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(&hwInfo));
    EXPECT_TRUE(device->isSimulation());
}

TEST(DeviceCreation, givenDeviceWhenCheckingGpgpuEnginesCountThenNumberGreaterThanZeroIsReturned) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_GT(hwHelper.getGpgpuEngineInstances(device->getHardwareInfo()).size(), 0u);
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

    for (auto ftrGpGpuMidThreadLevelPreempt : ::testing::Bool()) {
        defaultHwInfo->featureTable.flags.ftrGpGpuMidThreadLevelPreempt = ftrGpGpuMidThreadLevelPreempt;
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
}

using DeviceHwTest = ::testing::Test;

HWTEST_F(DeviceHwTest, givenHwHelperInputWhenInitializingCsrThenCreatePageTableManagerIfNeeded) {
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(3);
    executionEnvironment.incRefInternal();
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfo(&localHwInfo);
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment.initializeMemoryManager();
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
    struct MyHwHelper : HwHelperHw<FamilyType> {
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            if (engineType == aub_stream::ENGINE_RCS) {
                return EngineGroupType::RenderCompute;
            }
            if (EngineHelpers::isCcs(engineType)) {
                return EngineGroupType::Compute;
            }
            UNRECOVERABLE_IF(true);
        }
    };
    RAIIHwHelperFactory<MyHwHelper> overrideHwHelper{::defaultHwInfo->platform.eRenderCoreFamily};

    MockOsContext rcsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}));
    EngineControl rcsEngine{nullptr, &rcsContext};

    MockOsContext ccsContext(1, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::Regular}));
    EngineControl ccsEngine{nullptr, &ccsContext};

    MockDevice device{};
    ASSERT_EQ(0u, device.getRegularEngineGroups().size());
    device.addEngineToEngineGroup(ccsEngine);
    device.addEngineToEngineGroup(rcsEngine);
    auto &engineGroups = device.getRegularEngineGroups();
    EXPECT_EQ(1u, engineGroups[0].engines.size());
    EXPECT_EQ(EngineGroupType::Compute, engineGroups[0].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, engineGroups[0].engines[0].getEngineType());
    EXPECT_EQ(1u, engineGroups[1].engines.size());
    EXPECT_EQ(EngineGroupType::RenderCompute, engineGroups[1].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_RCS, engineGroups[1].engines[0].getEngineType());

    device.getRegularEngineGroups().clear();
    device.addEngineToEngineGroup(rcsEngine);
    device.addEngineToEngineGroup(ccsEngine);
    engineGroups = device.getRegularEngineGroups();
    EXPECT_EQ(1u, engineGroups[0].engines.size());
    EXPECT_EQ(EngineGroupType::RenderCompute, engineGroups[0].engineGroupType);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_RCS, engineGroups[0].engines[0].getEngineType());
    EXPECT_EQ(1u, engineGroups[1].engines.size());
    EXPECT_EQ(EngineGroupType::Compute, engineGroups[1].engineGroupType);
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
    DebugManager.flags.DeferOsContextInitialization.set(1);

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
    DebugManager.flags.DeferOsContextInitialization.set(0);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_NE(0u, device->getAllEngines().size());
    for (const EngineControl &engine : device->getAllEngines()) {
        EXPECT_TRUE(engine.osContext->isInitialized());
    }
}

TEST(DeviceGetEngineTest, givenNonHwCsrModeWhenGetEngineThenDefaultEngineIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

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
    EXPECT_EQ(static_cast<uint64_t>(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL), device->getQueueFamilyCapabilities(NEO::EngineGroupType::Compute));
    EXPECT_EQ(static_cast<uint64_t>(CL_QUEUE_DEFAULT_CAPABILITIES_INTEL), device->getQueueFamilyCapabilities(NEO::EngineGroupType::RenderCompute));
}

HWCMDTEST_F(IGFX_GEN8_CORE, QueueFamiliesTests, givenCopyQueueWhenGettingQueueFamilyCapabilitiesThenDoNotReturnUnsupportedOperations) {
    const cl_command_queue_capabilities_intel capabilitiesNotSupportedOnBlitter = CL_QUEUE_CAPABILITY_KERNEL_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    const cl_command_queue_capabilities_intel expectedBlitterCapabilities = setBits(MockClDevice::getQueueFamilyCapabilitiesAll(), false, capabilitiesNotSupportedOnBlitter);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(expectedBlitterCapabilities, device->getQueueFamilyCapabilities(NEO::EngineGroupType::Copy));
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
    DebugManager.flags.CreateMultipleSubDevices.set(0);

    uint32_t devicesCount = HwHelper::getSubDevicesCount(&hwInfo);
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
    DebugManager.flags.CreateMultipleSubDevices.set(0);

    uint32_t devicesCount = HwHelper::getSubDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 1u);
}

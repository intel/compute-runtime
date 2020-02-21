/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "test.h"

#include <memory>

using namespace NEO;

typedef Test<DeviceFixture> DeviceTest;

TEST_F(DeviceTest, givenDeviceWhenGetProductAbbrevThenReturnsHardwarePrefix) {
    const auto productAbbrev = pDevice->getProductAbbrev();
    const auto hwPrefix = hardwarePrefix[pDevice->getHardwareInfo().platform.eProductFamily];
    EXPECT_EQ(hwPrefix, productAbbrev);
}

TEST_F(DeviceTest, WhenDeviceIsCreatedThenCommandStreamReceiverIsNotNull) {
    EXPECT_NE(nullptr, &pDevice->getGpgpuCommandStreamReceiver());
}

TEST_F(DeviceTest, WhenDeviceIsCreatedThenSupportedClVersionMatchesHardwareInfo) {
    auto version = pDevice->getSupportedClVersion();
    auto version2 = pDevice->getHardwareInfo().capabilityTable.clVersionSupport;

    EXPECT_EQ(version, version2);
}

TEST_F(DeviceTest, givenDeviceWhenEngineIsCreatedThenSetInitialValueForTag) {
    for (auto &engine : pDevice->engines) {
        auto tagAddress = engine.commandStreamReceiver->getTagAddress();
        ASSERT_NE(nullptr, const_cast<uint32_t *>(tagAddress));
        EXPECT_EQ(initialHardwareTag, *tagAddress);
    }
}

TEST_F(DeviceTest, givenDeviceWhenAskedForSpecificEngineThenRetrunIt) {
    auto &engines = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0]);
    for (uint32_t i = 0; i < engines.size(); i++) {
        bool lowPriority = (HwHelper::lowPriorityGpgpuEngineIndex == i);
        auto &deviceEngine = pDevice->getEngine(engines[i], lowPriority);
        EXPECT_EQ(deviceEngine.osContext->getEngineType(), engines[i]);
        EXPECT_EQ(deviceEngine.osContext->isLowPriority(), lowPriority);
    }

    EXPECT_THROW(pDevice->getEngine(aub_stream::ENGINE_VCS, false), std::exception);
}

TEST_F(DeviceTest, givenDebugVariableToAlwaysChooseEngineZeroWhenNotExistingEngineSelectedThenIndexZeroEngineIsReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideInvalidEngineWithDefault.set(true);
    auto &engines = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0]);
    auto &deviceEngine = pDevice->getEngine(engines[0], false);
    auto &notExistingEngine = pDevice->getEngine(aub_stream::ENGINE_VCS, false);
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
    if (is64bit) {
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

TEST_F(DeviceTest, WhenAppendingOsExtensionsThenDeviceInfoIsProperlyUpdated) {
    EXPECT_NE(nullptr, pDevice);
    std::string testedValue = "1234!@#$";
    std::string expectedExtensions = pDevice->deviceExtensions + testedValue;

    pDevice->appendOSExtensions(testedValue);
    EXPECT_EQ(expectedExtensions, pDevice->deviceExtensions);
    EXPECT_STREQ(expectedExtensions.c_str(), pDevice->deviceInfo.deviceExtensions);
}

HWTEST_F(DeviceTest, WhenDeviceIsCreatedThenActualEngineTypeIsSameAsDefault) {
    HardwareInfo hwInfo = *platformDevices[0];
    if (hwInfo.capabilityTable.defaultEngineType == aub_stream::EngineType::ENGINE_CCS) {
        hwInfo.featureTable.ftrCCSNode = true;
    }

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    auto actualEngineType = device->getDefaultEngine().osContext->getEngineType();
    auto defaultEngineType = device->getHardwareInfo().capabilityTable.defaultEngineType;

    EXPECT_EQ(&device->getDefaultEngine().commandStreamReceiver->getOsContext(), device->getDefaultEngine().osContext);
    EXPECT_EQ(defaultEngineType, actualEngineType);
}

TEST(DeviceCleanup, givenDeviceWhenItIsDestroyedThenFlushBatchedSubmissionsIsCalled) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver *csr = new MockCommandStreamReceiver(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex());
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
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto memoryManager = device->getMemoryManager();
    auto &hwInfo = device->getHardwareInfo();
    auto numEnginesForDevice = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo).size();
    if (device->getNumAvailableDevices() > 1) {
        numEnginesForDevice *= device->getNumAvailableDevices();
        numEnginesForDevice += device->engines.size();
    }
    EXPECT_EQ(numEnginesForDevice, memoryManager->getRegisteredEnginesCount());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachOsContextHasUniqueId) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    auto hwInfo = executionEnvironment->getHardwareInfo();
    const auto &numGpgpuEngines = static_cast<uint32_t>(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo).size());

    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    auto &registeredEngines = executionEnvironment->memoryManager->getRegisteredEngines();
    EXPECT_EQ(numGpgpuEngines * numDevices, registeredEngines.size());

    for (uint32_t i = 0; i < numGpgpuEngines; i++) {
        EXPECT_EQ(i, device1->engines[i].osContext->getContextId());
        EXPECT_EQ(1u, device1->engines[i].osContext->getDeviceBitfield().to_ulong());
        EXPECT_EQ(i + numGpgpuEngines, device2->engines[i].osContext->getContextId());
        EXPECT_EQ(1u, device2->engines[i].osContext->getDeviceBitfield().to_ulong());

        EXPECT_EQ(registeredEngines[i].commandStreamReceiver,
                  device1->engines[i].commandStreamReceiver);
        EXPECT_EQ(registeredEngines[i + numGpgpuEngines].commandStreamReceiver,
                  device2->engines[i].commandStreamReceiver);
    }
    EXPECT_EQ(numGpgpuEngines * numDevices, executionEnvironment->memoryManager->getRegisteredEnginesCount());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachDeviceHasSeperateDeviceIndex) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    EXPECT_EQ(0u, device->getRootDeviceIndex());
    EXPECT_EQ(1u, device2->getRootDeviceIndex());
}

TEST(DeviceCreation, givenMultiRootDeviceWhenTheyAreCreatedThenEachDeviceHasSeperateCommandStreamReceiver) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    auto hwInfo = executionEnvironment->getHardwareInfo();
    const auto &numGpgpuEngines = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo).size();
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    EXPECT_EQ(numGpgpuEngines, device1->commandStreamReceivers.size());
    EXPECT_EQ(numGpgpuEngines, device2->commandStreamReceivers.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(numGpgpuEngines); i++) {
        EXPECT_NE(device2->engines[i].commandStreamReceiver, device1->engines[i].commandStreamReceiver);
    }
}

HWTEST_F(DeviceTest, givenDeviceWhenAskingForDefaultEngineThenReturnValidValue) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    hwHelper.adjustDefaultEngineType(executionEnvironment->getMutableHardwareInfo());

    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0));
    auto osContext = device->getDefaultEngine().osContext;

    EXPECT_EQ(executionEnvironment->getHardwareInfo()->capabilityTable.defaultEngineType, osContext->getEngineType());
    EXPECT_FALSE(osContext->isLowPriority());
}

TEST(DeviceCreation, givenFtrSimulationModeFlagTrueWhenNoOtherSimulationFlagsArePresentThenIsSimulationReturnsTrue) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.featureTable.ftrSimulationMode = true;

    bool simulationFromDeviceId = hwInfo.capabilityTable.isSimulation(hwInfo.platform.usDeviceID);
    EXPECT_FALSE(simulationFromDeviceId);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(&hwInfo));
    EXPECT_TRUE(device->isSimulation());
}

TEST(DeviceCreation, givenDeviceWhenCheckingEnginesCountThenNumberGreaterThanZeroIsReturned) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_GT(HwHelper::getEnginesCount(device->getHardwareInfo()), 0u);
}

using DeviceHwTest = ::testing::Test;

HWTEST_F(DeviceHwTest, givenHwHelperInputWhenInitializingCsrThenCreatePageTableManagerIfNeeded) {
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(3);
    executionEnvironment.incRefInternal();
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.setHwInfo(&localHwInfo);
    auto defaultEngineType = getChosenEngineType(localHwInfo);
    std::unique_ptr<MockDevice> device;
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&localHwInfo, &executionEnvironment, 0));
    auto &csr0 = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr0.createPageTableManagerCalled);

    auto hwInfo = executionEnvironment.getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo->capabilityTable.ftrRenderCompressedImages = false;
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&localHwInfo, &executionEnvironment, 1));
    auto &csr1 = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(csr1.needsPageTableManager(defaultEngineType), csr1.createPageTableManagerCalled);

    hwInfo->capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo->capabilityTable.ftrRenderCompressedImages = true;
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&localHwInfo, &executionEnvironment, 2));
    auto &csr2 = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(csr2.needsPageTableManager(defaultEngineType), csr2.createPageTableManagerCalled);
}

HWTEST_F(DeviceHwTest, givenDeviceCreationWhenCsrFailsToCreateGlobalSyncAllocationThenReturnNull) {
    class MockUltCsrThatFailsToCreateGlobalFenceAllocation : public UltCommandStreamReceiver<FamilyType> {
      public:
        MockUltCsrThatFailsToCreateGlobalFenceAllocation(ExecutionEnvironment &executionEnvironment)
            : UltCommandStreamReceiver<FamilyType>(executionEnvironment, 0) {}
        bool createGlobalFenceAllocation() override {
            return false;
        }
    };
    class MockDeviceThatFailsToCreateGlobalFenceAllocation : public MockDevice {
      public:
        MockDeviceThatFailsToCreateGlobalFenceAllocation(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
            : MockDevice(executionEnvironment, deviceIndex) {}
        std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override {
            return std::make_unique<MockUltCsrThatFailsToCreateGlobalFenceAllocation>(*executionEnvironment);
        }
    };

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    auto mockDevice(MockDevice::create<MockDeviceThatFailsToCreateGlobalFenceAllocation>(executionEnvironment, 0));
    EXPECT_EQ(nullptr, mockDevice);
}

TEST(DeviceGenEngineTest, givenHwCsrModeWhenGetEngineThenDedicatedForInternalUsageEngineIsReturned) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto &internalEngine = device->getInternalEngine();
    auto &defaultEngine = device->getDefaultEngine();
    EXPECT_NE(defaultEngine.commandStreamReceiver, internalEngine.commandStreamReceiver);

    auto internalEngineIndex = HwHelper::internalUsageEngineIndex;
    EXPECT_EQ(internalEngineIndex, internalEngine.osContext->getContextId());
}

TEST(DeviceGenEngineTest, whenCreateDeviceThenInternalEngineHasDefaultType) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto internalEngineType = device->getInternalEngine().osContext->getEngineType();
    auto defaultEngineType = getChosenEngineType(device->getHardwareInfo());
    EXPECT_EQ(defaultEngineType, internalEngineType);
}

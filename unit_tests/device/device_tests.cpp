/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include <memory>

using namespace OCLRT;

typedef Test<DeviceFixture> DeviceTest;

TEST_F(DeviceTest, Create) {
    EXPECT_NE(nullptr, pDevice);
}

TEST_F(DeviceTest, givenDeviceWhenGetProductAbbrevThenReturnsHardwarePrefix) {
    const auto productAbbrev = pDevice->getProductAbbrev();
    const auto hwPrefix = hardwarePrefix[pDevice->getHardwareInfo().pPlatform->eProductFamily];
    EXPECT_EQ(hwPrefix, productAbbrev);
}

TEST_F(DeviceTest, getCommandStreamReceiver) {
    EXPECT_NE(nullptr, &pDevice->getCommandStreamReceiver());
}

TEST_F(DeviceTest, getSupportedClVersion) {
    auto version = pDevice->getSupportedClVersion();
    auto version2 = pDevice->getHardwareInfo().capabilityTable.clVersionSupport;

    EXPECT_EQ(version, version2);
}

TEST_F(DeviceTest, getTagAddress) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(gpgpuEngineInstances.size()); i++) {
        auto tagAddress = pDevice->getEngine(i).commandStreamReceiver->getTagAddress();
        ASSERT_NE(nullptr, const_cast<uint32_t *>(tagAddress));
        EXPECT_EQ(initialHardwareTag, *tagAddress);
    }
}

TEST_F(DeviceTest, WhenGetOSTimeThenNotNull) {
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

TEST_F(DeviceTest, retainAndRelease) {
    ASSERT_NE(nullptr, pDevice);

    pDevice->retain();
    pDevice->retain();
    pDevice->retain();
    ASSERT_EQ(1, pDevice->getReference());

    ASSERT_FALSE(pDevice->release().isUnused());
    ASSERT_EQ(1, pDevice->getReference());
}

TEST_F(DeviceTest, getEngineTypeDefault) {
    auto pTestDevice = std::unique_ptr<Device>(createWithUsDeviceId(0));

    EngineType actualEngineType = pDevice->getDefaultEngine().osContext->getEngineType().type;
    EngineType defaultEngineType = pDevice->getHardwareInfo().capabilityTable.defaultEngineType;

    EXPECT_EQ(&pDevice->getDefaultEngine().commandStreamReceiver->getOsContext(), pDevice->getDefaultEngine().osContext);
    EXPECT_EQ(defaultEngineType, actualEngineType);
}

TEST(DeviceCleanup, givenDeviceWhenItIsDestroyedThenFlushBatchedSubmissionsIsCalled) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver *csr = new MockCommandStreamReceiver(*mockDevice->getExecutionEnvironment());
    mockDevice->resetCommandStreamReceiver(csr);
    int flushedBatchedSubmissionsCalledCount = 0;
    csr->flushBatchedSubmissionsCallCounter = &flushedBatchedSubmissionsCalledCount;
    mockDevice.reset(nullptr);

    EXPECT_EQ(1, flushedBatchedSubmissionsCalledCount);
}

TEST(DeviceCreation, givenSelectedAubCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsTrue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

    overrideCommandStreamReceiverCreation = true;
    auto mockDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_TRUE(mockDevice->isSimulation());
}

TEST(DeviceCreation, givenSelectedTbxCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsTrue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_TBX);

    overrideCommandStreamReceiverCreation = true;
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_TRUE(device->isSimulation());
}

TEST(DeviceCreation, givenSelectedTbxWithAubCsrInDebugVarsWhenDeviceIsCreatedThenIsSimulationReturnsTrue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_TBX_WITH_AUB);

    overrideCommandStreamReceiverCreation = true;
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
    int32_t defaultHwCsr = CommandStreamReceiverType::CSR_HW;
    EXPECT_EQ(defaultHwCsr, DebugManager.flags.SetCommandStreamReceiver.get());

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    EXPECT_FALSE(device->isSimulation());
}

TEST(DeviceCreation, givenDeviceWhenItIsCreatedThenOsContextIsRegistredInMemoryManager) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto memoryManager = device->getMemoryManager();
    EXPECT_EQ(gpgpuEngineInstances.size(), memoryManager->getOsContextCount());
}

TEST(DeviceCreation, givenMultiDeviceWhenTheyAreCreatedThenEachOsContextHasUniqueId) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    const size_t numDevices = 2;

    auto device1 = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 0u));
    auto device2 = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 1u));

    for (uint32_t i = 0; i < static_cast<uint32_t>(gpgpuEngineInstances.size()); i++) {
        EXPECT_EQ(i, device1->getEngine(i).osContext->getContextId());
        EXPECT_EQ(i + static_cast<uint32_t>(gpgpuEngineInstances.size()), device2->getEngine(i).osContext->getContextId());
    }
    EXPECT_EQ(gpgpuEngineInstances.size() * numDevices, executionEnvironment.memoryManager->getOsContextCount());
}

TEST(DeviceCreation, givenMultiDeviceWhenTheyAreCreatedThenEachDeviceHasSeperateDeviceIndex) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    auto device = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 0u));
    auto device2 = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 1u));

    EXPECT_EQ(0u, device->getDeviceIndex());
    EXPECT_EQ(1u, device2->getDeviceIndex());
}

TEST(DeviceCreation, givenMultiDeviceWhenTheyAreCreatedThenEachDeviceHasSeperateCommandStreamReceiver) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    const size_t numDevices = 2;
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 0u));
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    EXPECT_EQ(numDevices, executionEnvironment.commandStreamReceivers.size());
    EXPECT_EQ(gpgpuEngineInstances.size(), executionEnvironment.commandStreamReceivers[0].size());
    EXPECT_EQ(gpgpuEngineInstances.size(), executionEnvironment.commandStreamReceivers[1].size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(gpgpuEngineInstances.size()); i++) {
        EXPECT_NE(nullptr, executionEnvironment.commandStreamReceivers[0][i]);
        EXPECT_NE(nullptr, executionEnvironment.commandStreamReceivers[1][i]);
        EXPECT_EQ(executionEnvironment.commandStreamReceivers[0][i].get(), device1->getEngine(i).commandStreamReceiver);
        EXPECT_EQ(executionEnvironment.commandStreamReceivers[1][i].get(), device2->getEngine(i).commandStreamReceiver);
    }
}

TEST(DeviceCreation, givenDeviceWhenAskingForDefaultEngineThenReturnValidValue) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(platformDevices[0], &executionEnvironment, 0));

    auto &defaultEngine = device->getDefaultEngine().osContext->getEngineType();

    EXPECT_EQ(platformDevices[0]->capabilityTable.defaultEngineType, defaultEngine.type);
    EXPECT_EQ(0, defaultEngine.id);
}

TEST(DeviceCreation, givenFtrSimulationModeFlagTrueWhenNoOtherSimulationFlagsArePresentThenIsSimulationReturnsTrue) {
    FeatureTable skuTable = *platformDevices[0]->pSkuTable;
    skuTable.ftrSimulationMode = true;

    HardwareInfo hwInfo = {platformDevices[0]->pPlatform, &skuTable, platformDevices[0]->pWaTable,
                           platformDevices[0]->pSysInfo, platformDevices[0]->capabilityTable};

    int32_t defaultHwCsr = CommandStreamReceiverType::CSR_HW;
    EXPECT_EQ(defaultHwCsr, DebugManager.flags.SetCommandStreamReceiver.get());

    bool simulationFromDeviceId = hwInfo.capabilityTable.isSimulation(hwInfo.pPlatform->usDeviceID);
    EXPECT_FALSE(simulationFromDeviceId);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(&hwInfo));
    EXPECT_TRUE(device->isSimulation());
}

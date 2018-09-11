/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
    auto pTagAddress = pDevice->getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagAddress));
    EXPECT_EQ(initialHardwareTag, *pTagAddress);
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

    EngineType actualEngineType = pDevice->getEngineType();
    EngineType defaultEngineType = pDevice->getHardwareInfo().capabilityTable.defaultEngineType;

    EXPECT_EQ(defaultEngineType, actualEngineType);
}

TEST_F(DeviceTest, givenDebugVariableOverrideEngineTypeWhenDeviceIsCreatedThenUseDebugNotDefaul) {
    EngineType expectedEngine = EngineType::ENGINE_VCS;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(expectedEngine));
    auto pTestDevice = std::unique_ptr<Device>(createWithUsDeviceId(0));

    EngineType actualEngineType = pTestDevice->getEngineType();
    EngineType defaultEngineType = pDevice->getHardwareInfo().capabilityTable.defaultEngineType;

    EXPECT_NE(defaultEngineType, actualEngineType);
    EXPECT_EQ(expectedEngine, actualEngineType);
}

TEST(DeviceCleanup, givenDeviceWhenItIsDestroyedThenFlushBatchedSubmissionsIsCalled) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver *csr = new MockCommandStreamReceiver;
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
    EXPECT_EQ(1u, memoryManager->getOsContextCount());
}

TEST(DeviceCreation, givenMultiDeviceWhenTheyAreCreatedThenEachOsContextHasUniqueId) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    auto device = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 0u));
    auto device2 = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 1u));

    EXPECT_EQ(0u, device->getOsContext()->getContextId());
    EXPECT_EQ(1u, device2->getOsContext()->getContextId());
    EXPECT_EQ(2u, device->getMemoryManager()->getOsContextCount());
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
    auto device = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 0u));
    auto device2 = std::unique_ptr<Device>(Device::create<Device>(nullptr, &executionEnvironment, 1u));

    EXPECT_EQ(2u, executionEnvironment.commandStreamReceivers.size());
    EXPECT_NE(nullptr, executionEnvironment.commandStreamReceivers[0]);
    EXPECT_NE(nullptr, executionEnvironment.commandStreamReceivers[1]);
    EXPECT_EQ(&device->getCommandStreamReceiver(), executionEnvironment.commandStreamReceivers[0].get());
    EXPECT_EQ(&device2->getCommandStreamReceiver(), executionEnvironment.commandStreamReceivers[1].get());
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

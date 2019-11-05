/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/device/sub_device.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"
namespace NEO {
extern bool overrideDeviceWithDefaultHardwareInfo;
}
using namespace NEO;

TEST(SubDevicesTest, givenDefaultConfigWhenCreateRootDeviceThenItDoesntContainSubDevices) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(0u, device->getNumSubDevices());
    EXPECT_EQ(1u, device->getNumAvailableDevices());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenCreateRootDeviceThenItsSubdevicesHaveProperRootIdSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(2u, device->getNumSubDevices());
    EXPECT_EQ(0u, device->internalDeviceIndex);
    EXPECT_EQ(0u, device->getRootDeviceIndex());

    EXPECT_EQ(0u, device->subdevices.at(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, device->subdevices.at(0)->getSubDeviceIndex());
    EXPECT_EQ(1u, device->subdevices.at(0)->getInternalDeviceIndex());

    EXPECT_EQ(0u, device->subdevices.at(1)->getRootDeviceIndex());
    EXPECT_EQ(1u, device->subdevices.at(1)->getSubDeviceIndex());
    EXPECT_EQ(2u, device->subdevices.at(1)->getInternalDeviceIndex());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenCreateRootDeviceThenItContainsSubDevices) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(2u, device->getNumSubDevices());

    EXPECT_EQ(2u, device->getNumAvailableDevices());
    EXPECT_EQ(1u, device->subdevices.at(0)->getNumAvailableDevices());
    EXPECT_EQ(1u, device->subdevices.at(1)->getNumAvailableDevices());
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceRefcountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    auto subDevice0 = device->subdevices.at(0).get();
    auto subDevice1 = device->subdevices.at(1).get();
    auto baseApiRefCount = device->getRefApiCount();
    auto baseInternalRefCount = device->getRefInternalCount();

    subDevice0->retain();
    EXPECT_EQ(baseInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseApiRefCount, device->getRefApiCount());

    subDevice1->retainInternal();
    EXPECT_EQ(baseInternalRefCount + 2, device->getRefInternalCount());
    EXPECT_EQ(baseApiRefCount, device->getRefApiCount());

    subDevice0->release();
    EXPECT_EQ(baseInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseApiRefCount, device->getRefApiCount());

    subDevice1->releaseInternal();
    EXPECT_EQ(baseInternalRefCount, device->getRefInternalCount());
    EXPECT_EQ(baseApiRefCount, device->getRefApiCount());
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceCreationFailThenWholeDeviceIsDestroyed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(10);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    executionEnvironment.memoryManager.reset(new FailMemoryManager(10, executionEnvironment));
    auto device = Device::create<RootDevice>(&executionEnvironment, 0u);
    EXPECT_EQ(nullptr, device);
}

TEST(SubDevicesTest, givenCreateMultipleRootDevicesFlagsEnabledWhenDevicesAreCreatedThenEachHasUniqueDeviceIndex) {

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(2);
    VariableBackup<bool> backup(&overrideDeviceWithDefaultHardwareInfo, false);
    platform()->initialize();
    EXPECT_EQ(0u, platform()->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1u, platform()->getDevice(1)->getRootDeviceIndex());
}

TEST(SubDevicesTest, givenSubDeviceWhenOsContextIsCreatedThenItsBitfieldBasesOnSubDeviceId) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(2u, device->getNumSubDevices());

    auto firstSubDevice = static_cast<SubDevice *>(device->subdevices.at(0).get());
    auto secondSubDevice = static_cast<SubDevice *>(device->subdevices.at(1).get());
    uint32_t firstSubDeviceMask = (1u << 0);
    uint32_t secondSubDeviceMask = (1u << 1);
    EXPECT_EQ(firstSubDeviceMask, static_cast<uint32_t>(firstSubDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
    EXPECT_EQ(secondSubDeviceMask, static_cast<uint32_t>(secondSubDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
}

TEST(SubDevicesTest, givenDeviceWithoutSubDevicesWhenGettingDeviceByIdZeroThenGetThisDevice) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(1u, device->getNumAvailableDevices());
    EXPECT_EQ(device.get(), device->getDeviceById(0u));
    EXPECT_THROW(device->getDeviceById(1), std::exception);
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenGettingDeviceByIdThenGetCorrectSubDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
    EXPECT_EQ(2u, device->getNumSubDevices());
    EXPECT_EQ(device->subdevices.at(0).get(), device->getDeviceById(0));
    EXPECT_EQ(device->subdevices.at(1).get(), device->getDeviceById(1));
    EXPECT_THROW(device->getDeviceById(2), std::exception);
}

TEST(SubDevicesTest, givenSubDevicesWhenGettingDeviceByIdZeroThenGetThisSubDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
    EXPECT_EQ(2u, device->getNumSubDevices());
    auto subDevice = device->subdevices.at(0).get();

    EXPECT_EQ(subDevice, subDevice->getDeviceById(0));
    EXPECT_THROW(subDevice->getDeviceById(1), std::exception);
}

TEST(SubDevicesTest, givenRootDeviceWithSubdevicesWhenSetupRootEngineOnceAgainThenDontOverrideEngine) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
    EXPECT_EQ(2u, device->getNumSubDevices());
    auto subDevice = device->subdevices.at(0).get();

    auto subDeviceEngine = subDevice->getDefaultEngine();
    auto rootDeviceEngine = device->getDefaultEngine();

    EXPECT_NE(subDeviceEngine.commandStreamReceiver, rootDeviceEngine.commandStreamReceiver);
    EXPECT_NE(subDeviceEngine.osContext, rootDeviceEngine.osContext);

    device->setupRootEngine(subDeviceEngine);
    rootDeviceEngine = device->getDefaultEngine();

    EXPECT_NE(subDeviceEngine.commandStreamReceiver, rootDeviceEngine.commandStreamReceiver);
    EXPECT_NE(subDeviceEngine.osContext, rootDeviceEngine.osContext);
}

TEST(SubDevicesTest, givenRootDeviceWithoutEngineWhenSetupRootEngine) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.engines.size());
    EngineControl dummyEngine{reinterpret_cast<CommandStreamReceiver *>(0x123), reinterpret_cast<OsContext *>(0x456)};
    device.setupRootEngine(dummyEngine);
    EXPECT_EQ(1u, device.engines.size());
    EXPECT_EQ(dummyEngine.commandStreamReceiver, device.engines[0].commandStreamReceiver);
    EXPECT_EQ(dummyEngine.osContext, device.engines[0].osContext);

    device.engines.clear();
}

TEST(SubDevicesTest, givenRootDeviceWhenExecutionEnvironmentInitializesRootCommandStreamReceiverThenDeviceDoesntCreateExtraEngines) {
    auto executionEnvironment = new MockExecutionEnvironment;
    executionEnvironment->initRootCommandStreamReceiver = true;
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<MockDevice> device(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0u));
    EXPECT_EQ(0u, device->engines.size());
}

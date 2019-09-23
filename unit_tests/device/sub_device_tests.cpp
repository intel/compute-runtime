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

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenCreateRootDeviceThenItContainsSubDevices) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(2u, device->getNumSubDevices());

    EXPECT_EQ(0u, device->getDeviceIndex());
    EXPECT_EQ(1u, device->subdevices.at(0)->getDeviceIndex());
    EXPECT_EQ(2u, device->subdevices.at(1)->getDeviceIndex());

    EXPECT_EQ(2u, device->getNumAvailableDevices());
    EXPECT_EQ(1u, device->subdevices.at(0)->getNumAvailableDevices());
    EXPECT_EQ(1u, device->subdevices.at(1)->getNumAvailableDevices());
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceRefcountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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

TEST(SubDevicesTest, givenCreateMultipleDevicesFlagsEnabledWhenDevicesAreCreatedThenEachHasUniqueDeviceIndex) {

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleDevices.set(2);
    DebugManager.flags.CreateMultipleSubDevices.set(3);
    VariableBackup<bool> backup(&overrideDeviceWithDefaultHardwareInfo, false);
    platform()->initialize();
    EXPECT_EQ(0u, platform()->getDevice(0)->getDeviceIndex());
    EXPECT_EQ(4u, platform()->getDevice(1)->getDeviceIndex());
}

TEST(SubDevicesTest, givenSubDeviceWhenOsContextIsCreatedThenItsBitfieldBasesOnSubDeviceId) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    EXPECT_EQ(2u, device->getNumSubDevices());

    auto firstSubDevice = static_cast<SubDevice *>(device->subdevices.at(0).get());
    auto secondSubDevice = static_cast<SubDevice *>(device->subdevices.at(1).get());
    uint32_t firstSubDeviceMask = (1u << 0);
    uint32_t secondSubDeviceMask = (1u << 1);
    EXPECT_EQ(firstSubDeviceMask, static_cast<uint32_t>(firstSubDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
    EXPECT_EQ(secondSubDeviceMask, static_cast<uint32_t>(secondSubDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
}

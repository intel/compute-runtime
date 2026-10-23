/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/test/unit_tests/sources/linux/udev/mock_udev_os_library.h"

#include "gtest/gtest.h"

#include <memory>

namespace L0 {
namespace Sysman {
namespace ult {

static const std::vector<std::string> udevEntryPoints = {
    "udev_new",
    "udev_monitor_new_from_netlink",
    "udev_monitor_filter_add_match_subsystem_devtype",
    "udev_monitor_enable_receiving",
    "udev_monitor_get_fd",
    "udev_monitor_receive_device",
    "udev_device_get_devnum",
    "udev_device_get_action",
    "udev_device_get_property_value",
    "udev_device_unref"};

TEST(SysmanUdevLibTest, GivenLibraryLoadFailsWhenCreatingUdevLibThenNullptrIsReturned) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup(&NEO::OsLibrary::loadFunc, [](const NEO::OsLibraryCreateProperties &) -> NEO::OsLibrary * {
        return nullptr;
    });
    std::unique_ptr<L0::Sysman::UdevLib> pUdevLib(L0::Sysman::UdevLib::create());
    EXPECT_EQ(nullptr, pUdevLib);
}

TEST(SysmanUdevLibTest, GivenLibraryWithMissingEntryPointWhenCreatingUdevLibThenNullptrIsReturned) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup(&NEO::OsLibrary::loadFunc, [](const NEO::OsLibraryCreateProperties &) -> NEO::OsLibrary * {
        auto mockUdevOsLibrary = new MockUdevOsLibrary();
        mockUdevOsLibrary->deleteEntryPoint("udev_device_unref");
        return mockUdevOsLibrary;
    });
    std::unique_ptr<L0::Sysman::UdevLib> pUdevLib(L0::Sysman::UdevLib::create());
    EXPECT_EQ(nullptr, pUdevLib);
}

TEST(SysmanUdevLibTest, GivenUdevMonitorCreationFailsWhenCreatingUdevLibThenNullptrIsReturned) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup(&NEO::OsLibrary::loadFunc, MockUdevOsLibrary::load);
    VariableBackup<bool> monitorCreationFailsBackup(&MockUdevOsLibrary::mockMonitorCreationFails, true);
    std::unique_ptr<L0::Sysman::UdevLib> pUdevLib(L0::Sysman::UdevLib::create());
    EXPECT_EQ(nullptr, pUdevLib);
}

TEST(SysmanUdevLibTest, GivenLibraryWithAllEntryPointsWhenCreatingUdevLibThenUdevLibIsCreatedFromVersionedLibudev) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup(&NEO::OsLibrary::loadFunc, MockUdevOsLibrary::load);
    VariableBackup<std::string> loadedLibraryNameBackup(&MockUdevOsLibrary::loadedLibraryName, "");
    std::unique_ptr<L0::Sysman::UdevLib> pUdevLib(L0::Sysman::UdevLib::create());
    EXPECT_NE(nullptr, pUdevLib);
    EXPECT_EQ("libudev.so.1", MockUdevOsLibrary::loadedLibraryName);
}

TEST(SysmanUdevLibTest, GivenLibraryWithMissingEntryPointWhenLoadingEntryPointsThenFailureIsReturned) {
    for (const auto &entryPoint : udevEntryPoints) {
        L0::Sysman::UdevLibImp udevLibImp;
        auto mockUdevOsLibrary = std::make_unique<MockUdevOsLibrary>();
        mockUdevOsLibrary->deleteEntryPoint(entryPoint);
        udevLibImp.libraryHandle = std::move(mockUdevOsLibrary);
        EXPECT_FALSE(udevLibImp.loadEntryPoints()) << entryPoint;
    }
}

class SysmanUdevLibFixture : public ::testing::Test {
  protected:
    std::unique_ptr<L0::Sysman::UdevLib> pUdevLib;

    void SetUp() override {
        MockUdevOsLibrary::filterAddMatchSubsystemCallCount = 0;
        MockUdevOsLibrary::deviceUnrefCallCount = 0;
        VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup(&NEO::OsLibrary::loadFunc, MockUdevOsLibrary::load);
        pUdevLib.reset(L0::Sysman::UdevLib::create());
        ASSERT_NE(nullptr, pUdevLib);
    }
};

TEST_F(SysmanUdevLibFixture, GivenSubsystemListWhenRegisteringEventsThenEachSubsystemIsAddedToMonitorFilterAndMonitorFdIsReturned) {
    std::vector<std::string> subsystemList = MockUdevOsLibrary::mockSubsystems;
    EXPECT_EQ(MockUdevOsLibrary::mockMonitorFd, pUdevLib->registerEventsFromSubsystemAndGetFd(subsystemList));
    EXPECT_EQ(subsystemList.size(), MockUdevOsLibrary::filterAddMatchSubsystemCallCount);
}

TEST_F(SysmanUdevLibFixture, GivenEnableReceivingFailsWhenRegisteringEventsThenErrorIsReturned) {
    VariableBackup<int> enableReceivingResultBackup(&MockUdevOsLibrary::mockEnableReceivingResult, -1);
    std::vector<std::string> subsystemList = MockUdevOsLibrary::mockSubsystems;
    EXPECT_EQ(-1, pUdevLib->registerEventsFromSubsystemAndGetFd(subsystemList));
}

TEST_F(SysmanUdevLibFixture, GivenUdevLibWhenAllocatingDeviceToReceiveDataThenDeviceReceivedFromMonitorIsReturned) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevDevice, pUdevLib->allocateDeviceToReceiveData());
}

TEST_F(SysmanUdevLibFixture, GivenReceivedDeviceWhenGettingEventGenerationSourceDeviceThenDeviceNumberIsReturned) {
    void *dev = pUdevLib->allocateDeviceToReceiveData();
    EXPECT_EQ(MockUdevOsLibrary::mockDevnum, pUdevLib->getEventGenerationSourceDevice(dev));
}

TEST_F(SysmanUdevLibFixture, GivenReceivedDeviceWhenGettingEventTypeThenDeviceActionIsReturned) {
    void *dev = pUdevLib->allocateDeviceToReceiveData();
    EXPECT_STREQ(MockUdevOsLibrary::mockAction, pUdevLib->getEventType(dev));
}

TEST_F(SysmanUdevLibFixture, GivenReceivedDeviceWhenGettingEventPropertyValueThenValueOfRequestedPropertyIsReturned) {
    void *dev = pUdevLib->allocateDeviceToReceiveData();
    EXPECT_STREQ(MockUdevOsLibrary::mockPropertyValue, pUdevLib->getEventPropertyValue(dev, MockUdevOsLibrary::mockPropertyKey));
}

TEST_F(SysmanUdevLibFixture, GivenReceivedDeviceWhenDroppingDeviceReferenceThenDeviceIsUnreferenced) {
    void *dev = pUdevLib->allocateDeviceToReceiveData();
    pUdevLib->dropDeviceReference(dev);
    EXPECT_EQ(1u, MockUdevOsLibrary::deviceUnrefCallCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

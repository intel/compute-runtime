/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/udev/mock_udev_os_library.h"

#include "gtest/gtest.h"

#include <sys/sysmacros.h>

namespace L0 {
namespace Sysman {
namespace ult {

struct udev *const MockUdevOsLibrary::mockUdev = reinterpret_cast<struct udev *>(0x8);
struct udev_monitor *const MockUdevOsLibrary::mockUdevMonitor = reinterpret_cast<struct udev_monitor *>(0x10);
struct udev_device *const MockUdevOsLibrary::mockUdevDevice = reinterpret_cast<struct udev_device *>(0x18);
bool MockUdevOsLibrary::mockMonitorCreationFails = false;
int MockUdevOsLibrary::mockEnableReceivingResult = 0;
int MockUdevOsLibrary::mockMonitorFd = 42;
dev_t MockUdevOsLibrary::mockDevnum = makedev(226, 128);
const char *MockUdevOsLibrary::mockAction = "change";
const char *MockUdevOsLibrary::mockPropertyKey = "RESET_FAILED";
const char *MockUdevOsLibrary::mockPropertyValue = "1";
std::string MockUdevOsLibrary::loadedLibraryName;
const std::vector<std::string> MockUdevOsLibrary::mockSubsystems = {"drm", "auxiliary"};
uint32_t MockUdevOsLibrary::filterAddMatchSubsystemCallCount = 0;
uint32_t MockUdevOsLibrary::deviceUnrefCallCount = 0;

extern "C" {

struct udev *mockUdevNew() {
    return MockUdevOsLibrary::mockUdev;
}

struct udev_monitor *mockUdevMonitorNewFromNetlink(struct udev *udev, const char *name) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdev, udev);
    EXPECT_STREQ("kernel", name);
    if (MockUdevOsLibrary::mockMonitorCreationFails) {
        return nullptr;
    }
    return MockUdevOsLibrary::mockUdevMonitor;
}

int mockUdevMonitorFilterAddMatchSubsystemDevtype(struct udev_monitor *udevMonitor, const char *subsystem, const char *devtype) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevMonitor, udevMonitor);
    EXPECT_EQ(nullptr, devtype);
    EXPECT_LT(MockUdevOsLibrary::filterAddMatchSubsystemCallCount, MockUdevOsLibrary::mockSubsystems.size());
    if (MockUdevOsLibrary::filterAddMatchSubsystemCallCount < MockUdevOsLibrary::mockSubsystems.size()) {
        EXPECT_STREQ(MockUdevOsLibrary::mockSubsystems[MockUdevOsLibrary::filterAddMatchSubsystemCallCount].c_str(), subsystem);
    }
    MockUdevOsLibrary::filterAddMatchSubsystemCallCount++;
    return 0;
}

int mockUdevMonitorEnableReceiving(struct udev_monitor *udevMonitor) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevMonitor, udevMonitor);
    return MockUdevOsLibrary::mockEnableReceivingResult;
}

int mockUdevMonitorGetFd(struct udev_monitor *udevMonitor) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevMonitor, udevMonitor);
    return MockUdevOsLibrary::mockMonitorFd;
}

struct udev_device *mockUdevMonitorReceiveDevice(struct udev_monitor *udevMonitor) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevMonitor, udevMonitor);
    return MockUdevOsLibrary::mockUdevDevice;
}

dev_t mockUdevDeviceGetDevnum(struct udev_device *udevDevice) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevDevice, udevDevice);
    return MockUdevOsLibrary::mockDevnum;
}

const char *mockUdevDeviceGetAction(struct udev_device *udevDevice) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevDevice, udevDevice);
    return MockUdevOsLibrary::mockAction;
}

const char *mockUdevDeviceGetPropertyValue(struct udev_device *udevDevice, const char *key) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevDevice, udevDevice);
    EXPECT_STREQ(MockUdevOsLibrary::mockPropertyKey, key);
    return MockUdevOsLibrary::mockPropertyValue;
}

struct udev_device *mockUdevDeviceUnref(struct udev_device *udevDevice) {
    EXPECT_EQ(MockUdevOsLibrary::mockUdevDevice, udevDevice);
    MockUdevOsLibrary::deviceUnrefCallCount++;
    return nullptr;
}
}

MockUdevOsLibrary::MockUdevOsLibrary() {
    funcMap["udev_new"] = reinterpret_cast<void *>(&mockUdevNew);
    funcMap["udev_monitor_new_from_netlink"] = reinterpret_cast<void *>(&mockUdevMonitorNewFromNetlink);
    funcMap["udev_monitor_filter_add_match_subsystem_devtype"] = reinterpret_cast<void *>(&mockUdevMonitorFilterAddMatchSubsystemDevtype);
    funcMap["udev_monitor_enable_receiving"] = reinterpret_cast<void *>(&mockUdevMonitorEnableReceiving);
    funcMap["udev_monitor_get_fd"] = reinterpret_cast<void *>(&mockUdevMonitorGetFd);
    funcMap["udev_monitor_receive_device"] = reinterpret_cast<void *>(&mockUdevMonitorReceiveDevice);
    funcMap["udev_device_get_devnum"] = reinterpret_cast<void *>(&mockUdevDeviceGetDevnum);
    funcMap["udev_device_get_action"] = reinterpret_cast<void *>(&mockUdevDeviceGetAction);
    funcMap["udev_device_get_property_value"] = reinterpret_cast<void *>(&mockUdevDeviceGetPropertyValue);
    funcMap["udev_device_unref"] = reinterpret_cast<void *>(&mockUdevDeviceUnref);
}

void *MockUdevOsLibrary::getProcAddress(const std::string &procName) {
    auto it = funcMap.find(procName);
    if (funcMap.end() == it) {
        return nullptr;
    } else {
        return it->second;
    }
}

void MockUdevOsLibrary::deleteEntryPoint(const std::string &procName) {
    funcMap.erase(procName);
}

NEO::OsLibrary *MockUdevOsLibrary::load(const NEO::OsLibraryCreateProperties &properties) {
    loadedLibraryName = properties.libraryName;
    return new MockUdevOsLibrary();
}

} // namespace ult
} // namespace Sysman
} // namespace L0

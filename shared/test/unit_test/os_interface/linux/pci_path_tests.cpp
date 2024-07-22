/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenGettingPciPathThenProperPathIsReturned) {
    static const int fileDescriptorRender = 0x124;
    static const int fileDescriptorRenderInvalid = 0x125;
    static const int fileDescriptorUnknown = 0x126;

    static const char devicePathRender[] = "devicePathRender";
    static const char devicePathRenderInvalid[] = "devicePathRenderInvalid";
    static const char devicePathUnknown[] = "devicePathUnk";

    static const char linkPathRender[] = "../../devices/pci0000:4a/0000:4b:00.0/0000:4c:01.0/0001:02:03.4/drm/renderD128";
    static const char linkPathRenderInvalid[] = "../../drm/renderD128";
    static const char linkPathUnknown[] = "../../devices/pci0000:4a/0000:4b:00.0/0000:4c:01.0/0001:02:03.4/drm/unknown";

    static const char outPciPath[] = "0001:02:03.4";

    VariableBackup<bool> allowFakeDevicePathBackup{&SysCalls::allowFakeDevicePath, true};
    VariableBackup<decltype(SysCalls::sysCallsGetDevicePath)> getDevicePathBackup(&SysCalls::sysCallsGetDevicePath);
    VariableBackup<decltype(SysCalls::sysCallsReadlink)> readlinkBackup(&SysCalls::sysCallsReadlink);

    SysCalls::sysCallsGetDevicePath = [](int deviceFd, char *buf, size_t &bufSize) -> int {
        if (deviceFd == fileDescriptorRender) {
            bufSize = sizeof(devicePathRender);
            strcpy_s(buf, bufSize, devicePathRender);
            return 0;
        }
        if (deviceFd == fileDescriptorRenderInvalid) {
            bufSize = sizeof(devicePathRenderInvalid);
            strcpy_s(buf, bufSize, devicePathRenderInvalid);
            return 0;
        }
        if (deviceFd == fileDescriptorUnknown) {
            bufSize = sizeof(devicePathUnknown);
            strcpy_s(buf, bufSize, devicePathUnknown);
            return 0;
        }
        return -1;
    };

    SysCalls::sysCallsReadlink = [](const char *path, char *buf, size_t bufSize) -> int {
        if (strcmp(path, devicePathRender) == 0) {
            bufSize = sizeof(linkPathRender);
            strcpy_s(buf, bufSize, linkPathRender);
            return static_cast<int>(bufSize);
        }
        if (strcmp(path, devicePathRenderInvalid) == 0) {
            bufSize = sizeof(linkPathRenderInvalid);
            strcpy_s(buf, bufSize, linkPathRenderInvalid);
            return static_cast<int>(bufSize);
        }
        if (strcmp(path, devicePathUnknown) == 0) {
            bufSize = sizeof(linkPathUnknown);
            strcpy_s(buf, bufSize, linkPathUnknown);
            return static_cast<int>(bufSize);
        }
        return -1;
    };

    auto pciPath = getPciPath(fileDescriptorUnknown);
    EXPECT_FALSE(pciPath.has_value());

    pciPath = getPciPath(fileDescriptorRenderInvalid);
    EXPECT_FALSE(pciPath.has_value());

    pciPath = getPciPath(fileDescriptorRender);
    EXPECT_TRUE(pciPath.has_value());
    EXPECT_EQ(outPciPath, *pciPath);
}

TEST(DrmTest, whenGettingPciPathContainsCard0ThenProperPathIsReturned) {
    static const int fileDescriptorCard = 0x127;

    static const char devicePathCard[] = "devicePathCard";

    static const char linkPathCard[] = "../../devices/pci0000:4a/0000:4b:00.0/0000:4c:01.0/0001:02:03.4/drm/card0";

    static const char outPciPath[] = "0001:02:03.4";

    VariableBackup<bool> allowFakeDevicePathBackup{&SysCalls::allowFakeDevicePath, true};
    VariableBackup<decltype(SysCalls::sysCallsGetDevicePath)> getDevicePathBackup(&SysCalls::sysCallsGetDevicePath);
    VariableBackup<decltype(SysCalls::sysCallsReadlink)> readlinkBackup(&SysCalls::sysCallsReadlink);

    SysCalls::sysCallsGetDevicePath = [](int deviceFd, char *buf, size_t &bufSize) -> int {
        if (deviceFd == fileDescriptorCard) {
            bufSize = sizeof(devicePathCard);
            strcpy_s(buf, bufSize, devicePathCard);
            return 0;
        }
        return -1;
    };

    SysCalls::sysCallsReadlink = [](const char *path, char *buf, size_t bufSize) -> int {
        if (strcmp(path, devicePathCard) == 0) {
            bufSize = sizeof(linkPathCard);
            strcpy_s(buf, bufSize, linkPathCard);
            return static_cast<int>(bufSize);
        }
        return -1;
    };

    auto pciPath = getPciPath(fileDescriptorCard);
    EXPECT_TRUE(pciPath.has_value());
    EXPECT_EQ(outPciPath, *pciPath);
}

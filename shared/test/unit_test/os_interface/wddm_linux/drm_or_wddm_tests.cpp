/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
extern bool returnEmptyFilesVector;
extern int setErrno;
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

TEST(DrmOrWddmTest, GivenAccessDeniedWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EACCES;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenInufficientPermissionsWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EPERM;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenOtherErrorWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenAccessDeniedWithNonEmptyFilesListWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EACCES;
        return -1;
    });

    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:01.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:02.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:03.0-render", {"unknown"}});

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);

    NEO::directoryFilesMap.clear();
}

TEST(DrmOrWddmTest, GivenInufficientPermissionsWithNonEmptyFilesListWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EPERM;
        return -1;
    });

    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:01.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:02.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:03.0-render", {"unknown"}});

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);

    NEO::directoryFilesMap.clear();
}

TEST(DrmOrWddmTest, GivenAccessDeniedForDirectoryWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);
    VariableBackup<int> errnoNumber(&NEO::setErrno, EACCES);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenInufficientPermissionsForDirectoryWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);
    VariableBackup<int> errnoNumber(&NEO::setErrno, EPERM);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}
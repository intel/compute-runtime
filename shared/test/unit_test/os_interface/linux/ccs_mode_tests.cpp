/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
};

static uint32_t ccsMode = 1u;

static ssize_t mockWrite(int fd, const void *buf, size_t count) {
    std::memcpy(&ccsMode, buf, count);
    return count;
}

TEST(CcsModeTest, GivenInvalidCcsModeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("version");

    directoryFilesMap["/sys/class/drm/card0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/gt"].push_back("/sys/class/drm/card0/gt/gt0");
    directoryFilesMap["/sys/class/drm/card0/gt"].push_back("unknown");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;
    executionEnvironment.configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("abc");
    executionEnvironment.configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("");
    executionEnvironment.configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenValidCcsModeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsProperlyConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("version");

    directoryFilesMap["/sys/class/drm/card0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/gt"].push_back("/sys/class/drm/card0/gt/gt0");
    directoryFilesMap["/sys/class/drm/card0/gt"].push_back("unknown");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment.configureCcsMode();

    EXPECT_EQ(2u, ccsMode);
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenValidCcsModeAndOpenSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = -ENOENT;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment.configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenValidCcsModeAndOpenSysCallFailsWithNoPermissionsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = -EPERM;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment.configureCcsMode();

    std::string stdOutString = testing::internal::GetCapturedStdout();
    std::string stdErrString = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "No read and write permissions for /sys/class/drm/card0/gt/gt0/ccs_mode, System administrator needs to grant permissions to allow modification of this file from user space\n";

    EXPECT_EQ(1u, ccsMode);
    EXPECT_STREQ(stdOutString.c_str(), expectedOutput.c_str());
    EXPECT_STREQ(stdErrString.c_str(), expectedOutput.c_str());
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenValidCcsModeAndOpenSysCallFailsWithNoAccessWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = -EACCES;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment.configureCcsMode();

    std::string stdOutString = testing::internal::GetCapturedStdout();
    std::string stdErrString = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "No read and write permissions for /sys/class/drm/card0/gt/gt0/ccs_mode, System administrator needs to grant permissions to allow modification of this file from user space\n";

    EXPECT_EQ(1u, ccsMode);
    EXPECT_STREQ(stdOutString.c_str(), expectedOutput.c_str());
    EXPECT_STREQ(stdErrString.c_str(), expectedOutput.c_str());
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenNumCCSFlagSetToCurrentConfigurationWhenConfigureCcsModeIsCalledThenVerifyWriteCallIsNotInvoked) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        SysCalls::writeFuncCalled++;
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("1");
    executionEnvironment.configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    EXPECT_EQ(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenNumCCSFlagSetToCurrentConfigurationAndReadSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        SysCalls::writeFuncCalled++;
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        return -1;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment.configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    EXPECT_EQ(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenValidCcsModeAndWriteSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        SysCalls::writeFuncCalled++;
        errno = -EAGAIN;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    NEO::ExecutionEnvironment executionEnvironment;

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment.configureCcsMode();

    EXPECT_NE(2u, ccsMode);
    EXPECT_NE(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenWriteSysCallFailsWithEbusyForFirstTimeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsConfiguredInSecondIteration) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/gt"] = {"/sys/class/drm/card0/gt/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        if (SysCalls::writeFuncCalled++ == 0u) {
            errno = -EBUSY;
            return -1;
        }
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    {
        NEO::ExecutionEnvironment executionEnvironment;

        DebugManagerStateRestore restorer;
        debugManager.flags.ZEX_NUMBER_OF_CCS.set("4");
        executionEnvironment.configureCcsMode();

        EXPECT_EQ(4u, ccsMode);
        EXPECT_NE(0u, SysCalls::writeFuncCalled);
    }
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST(CcsModeTest, GivenValidCcsModeAndOpenCallFailsOnRestoreWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotRestored) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("version");

    directoryFilesMap["/sys/class/drm/card0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/gt"].push_back("/sys/class/drm/card0/gt/gt0");
    directoryFilesMap["/sys/class/drm/card0/gt"].push_back("unknown");

    SysCalls::openFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (SysCalls::openFuncCalled == 1u) {
            return 1;
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    {
        NEO::ExecutionEnvironment executionEnvironment;

        DebugManagerStateRestore restorer;
        debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
        executionEnvironment.configureCcsMode();

        EXPECT_EQ(2u, ccsMode);
    }

    EXPECT_NE(1u, ccsMode);
    directoryFilesMap.clear();
}

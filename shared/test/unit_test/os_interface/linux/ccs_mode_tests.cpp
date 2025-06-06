/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
};

static uint32_t ccsMode = 1u;

static ssize_t mockWrite(int fd, const void *buf, size_t count) {
    std::memcpy(&ccsMode, buf, count);
    return count;
}

class CcsModePrelimFixture {
  public:
    void setUp() {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }

    void tearDown() {
    }
    DrmMock *drm;
    OSInterface *osInterface;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

using CcsModeTest = Test<CcsModePrelimFixture>;

TEST_F(CcsModeTest, GivenInvalidCcsModeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("abc");
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("");
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenValidCcsModeWhenConfigureCcsModeIsCalledWithoutRootDeviceEnvironmentsThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->rootDeviceEnvironments.clear();
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);
    executionEnvironment->prepareRootDeviceEnvironments(1);

    directoryFilesMap.clear();
}

class DefaultDriverModelMock : public MockDriverModel {
  public:
    DefaultDriverModelMock(DriverModelType driverModelType) : MockDriverModel(driverModelType) {
    }

    bool isDriverAvailable() override {
        return true;
    }
    void setGmmInputArgs(void *args) override {
    }

    uint32_t getDeviceHandle() const override {
        return 0;
    }

    PhysicalDevicePciBusInfo getPciBusInfo() const override {
        return {};
    }
    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return {};
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    bool isGpuHangDetected(OsContext &osContext) override {
        return false;
    }
};

TEST_F(CcsModeTest, GivenValidCcsModeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsProperlyConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");
    directoryFilesMap["/sys/class/drm"].push_back("version");

    directoryFilesMap["/sys/class/drm/card1/gt"] = {};
    directoryFilesMap["/sys/class/drm/card1/gt"].push_back("/sys/class/drm/card1/gt/gt0");
    directoryFilesMap["/sys/class/drm/card1/gt"].push_back("unknown");

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

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(2u, ccsMode);
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenValidCcsModeAndOpenSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenValidCcsModeAndOpenSysCallFailsWithNoPermissionsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;

    StreamCapture capture;
    capture.captureStdout();
    testing::internal::CaptureStderr();

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    std::string stdOutString = capture.getCapturedStdout();
    std::string stdErrString = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "No read and write permissions for /sys/class/drm/card0/gt/gt0/ccs_mode, System administrator needs to grant permissions to allow modification of this file from user space\n";

    EXPECT_EQ(1u, ccsMode);
    EXPECT_STREQ(stdOutString.c_str(), expectedOutput.c_str());
    EXPECT_STREQ(stdErrString.c_str(), expectedOutput.c_str());
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenValidCcsModeAndOpenSysCallFailsWithNoAccessWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;

    StreamCapture capture;
    capture.captureStdout();
    testing::internal::CaptureStderr();

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    std::string stdOutString = capture.getCapturedStdout();
    std::string stdErrString = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "No read and write permissions for /sys/class/drm/card0/gt/gt0/ccs_mode, System administrator needs to grant permissions to allow modification of this file from user space\n";

    EXPECT_EQ(1u, ccsMode);
    EXPECT_STREQ(stdOutString.c_str(), expectedOutput.c_str());
    EXPECT_STREQ(stdErrString.c_str(), expectedOutput.c_str());
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenNumCCSFlagSetToCurrentConfigurationWhenConfigureCcsModeIsCalledThenVerifyWriteCallIsNotInvoked) {
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

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("1");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    EXPECT_EQ(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenNumCCSFlagSetToCurrentConfigurationAndReadSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    EXPECT_EQ(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenValidCcsModeAndWriteSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
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

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_NE(2u, ccsMode);
    EXPECT_NE(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenWriteSysCallFailsWithEbusyForFirstTimeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsConfiguredInSecondIteration) {
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
        DebugManagerStateRestore restorer;
        debugManager.flags.ZEX_NUMBER_OF_CCS.set("4");
        executionEnvironment->configureCcsMode();

        EXPECT_EQ(4u, ccsMode);
        EXPECT_NE(0u, SysCalls::writeFuncCalled);
    }
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeTest, GivenValidCcsModeAndOpenCallFailsOnRestoreWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotRestored) {
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
        DebugManagerStateRestore restorer;
        debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
        executionEnvironment->configureCcsMode();

        EXPECT_EQ(2u, ccsMode);
    }

    EXPECT_NE(1u, ccsMode);
    directoryFilesMap.clear();
}

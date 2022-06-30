/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct DrmPciSpeedInfoTest : public ::testing::Test {

    void SetUp() override {

        SysCalls::sysCallsReadlink = mockReadLink;
        SysCalls::sysCallsOpen = mockOpenDiscrete;
        SysCalls::sysCallsPread = mockPreadDiscrete;
    }
    void TearDown() override {
        SysCalls::sysCallsReadlink = sysCallsReadlinkBackup;
        SysCalls::sysCallsOpen = sysCallsOpenBackup;
        SysCalls::sysCallsPread = sysCallsPreadBackup;
    }

    std::unique_ptr<DrmMock> getDrm(bool isTestIntegratedDevice) {
        HardwareInfo hwInfo = *defaultHwInfo.get();
        hwInfo.capabilityTable.isIntegratedDevice = isTestIntegratedDevice;
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(&hwInfo, false, 1);
        return std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    }

    decltype(SysCalls::sysCallsReadlink) mockReadLink = [](const char *path, char *buf, size_t bufsize) -> int {
        std::string linkString("../../devices/pci0000:00/0000:00:02.0/0000:00:02.0/0000:00:02.0/drm/renderD128");
        memcpy_s(buf, bufsize, linkString.c_str(), linkString.size());
        return static_cast<int>(linkString.size());
    };

    decltype(SysCalls::sysCallsOpen) mockOpenDiscrete = [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/devices/pci0000:00/0000:00:02.0/max_link_width",
            "/sys/devices/pci0000:00/0000:00:02.0/max_link_speed"};
        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    };

    decltype(SysCalls::sysCallsPread) mockPreadDiscrete = [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "64"},
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_speed", "32"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    };

    decltype(SysCalls::sysCallsReadlink) sysCallsReadlinkBackup = SysCalls::sysCallsReadlink;
    decltype(SysCalls::sysCallsOpen) sysCallsOpenBackup = SysCalls::sysCallsOpen;
    decltype(SysCalls::sysCallsPread) sysCallsPreadBackup = SysCalls::sysCallsPread;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment = nullptr;
};

TEST_F(DrmPciSpeedInfoTest, givenIntegratedDeviceWhenCorrectInputsAreGivenThenCorrectPciSpeedIsReturned) {

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpenIntegrated(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/drm/../../devices/pci0000:00/0000:00:02.0/0000:00:02.0/0000:00:02.0/drm/renderD128/device//max_link_width",
            "/sys/class/drm/../../devices/pci0000:00/0000:00:02.0/0000:00:02.0/0000:00:02.0/drm/renderD128/device//max_link_speed"};
        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });
    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPreadIntegrated(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/drm/../../devices/pci0000:00/0000:00:02.0/0000:00:02.0/0000:00:02.0/drm/renderD128/device//max_link_width", "64"},
            {"/sys/class/drm/../../devices/pci0000:00/0000:00:02.0/0000:00:02.0/0000:00:02.0/drm/renderD128/device//max_link_speed", "16.0 GT/s PCIe \n test"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(true)->getPciSpeedInfo();
    EXPECT_EQ(4, pciSpeedInfo.genVersion);
    EXPECT_EQ(64, pciSpeedInfo.width);
    EXPECT_EQ(126030769230, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenCorrectInputsAreGivenThenCorrectPciSpeedIsReturned) {

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(5, pciSpeedInfo.genVersion);
    EXPECT_EQ(64, pciSpeedInfo.width);
    EXPECT_EQ(252061538461, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenIntegratedDeviceWhenPciLinkPathFailsThenUnknownPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(true)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenPciRootPathFailsThenUnknownPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenPciRootPathFailsDueToMissingPciThenUnknownPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::string linkString("../../devices/");
        memcpy_s(buf, bufsize, linkString.c_str(), linkString.size());
        return static_cast<int>(linkString.size());
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenPciRootPathFailsDueToUnavailableDirectoryPathThenUnknownPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::string linkString("../../devices/pci0000:00");
        memcpy_s(buf, bufsize, linkString.c_str(), linkString.size());
        return static_cast<int>(linkString.size());
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenPreadReturnsNegativeValueWhenAccessingSysfsNodesThenUnknownPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenPreadReturnsZeroWhenAccessingSysfsNodesThenUnknownPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return 0;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenReadingLinkWidthStrtoulFailsWithNoValidConversionUnknownThenPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "Unknown"},
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_speed", "32"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenReadingLinkWidthStrtoulFailsWithErrnoSetUnknownThenPciSpeedIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "64"},
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_speed", "32"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            errno = 22;
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(-1, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenReadingMaxLinkSpeedFailsThenUnknownPciSpeedIsReturnedForGenVersionAndMaxBandwidth) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "64"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(64, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenReadingLinkSpeedStrtoulFailsWithNoValidConversionThenUnknownPciSpeedIsReturnedForGenVersionAndMaxBandwidth) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "64"},
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_speed", "Unknown"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(64, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenReadingLinkSpeedStrtodFailsWithErrnoSetThenUnknownPciSpeedIsReturnedForGenVersionAndMaxBandwidth) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "64"},
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_speed", "32"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            if (fd == 1) {
                errno = 22;
            }
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(64, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}

TEST_F(DrmPciSpeedInfoTest, givenDiscreteDeviceWhenUnSupportedLinkSpeedIsReadThenUnknownPciSpeedIsReturnedForGenVersionAndMaxBandwidth) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_width", "64"},
            {"/sys/devices/pci0000:00/0000:00:02.0/max_link_speed", "0 32"},
        };
        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, count, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    PhyicalDevicePciSpeedInfo pciSpeedInfo = getDrm(false)->getPciSpeedInfo();
    EXPECT_EQ(-1, pciSpeedInfo.genVersion);
    EXPECT_EQ(64, pciSpeedInfo.width);
    EXPECT_EQ(-1, pciSpeedInfo.maxBandwidth);
}
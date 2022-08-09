/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/root_device.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

struct MultipleDeviceUuidTest : public ::testing::Test {
    void SetUp() override {

        ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        std::vector<std::string> pciPaths = {
            "0000:3a:00.0",
            "0000:9b:00.0"};

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<Device>(deviceFactory->rootDevices[i]));

            auto execEnv = deviceFactory->rootDevices[i]->getExecutionEnvironment();
            auto rootEnv = execEnv->rootDeviceEnvironments[i].get();
            rootEnv->osInterface.reset(new OSInterface);
            auto drmMock = new DrmMockResources(*execEnv->rootDeviceEnvironments[i]);
            execEnv->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
            drmMock->setPciPath(pciPaths[i % pciPaths.size()].c_str());
        }
    }

    void TearDown() override {
        for (auto &device : devices) {
            device.release();
        }
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<Device>> devices;
};
} // namespace NEO

using namespace NEO;
HWTEST2_F(MultipleDeviceUuidTest, whenRetrievingDeviceUuidThenCorrectUuidIsReceived, IsXEHP) {
    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/crashlog1", "../../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-4.4.auto/intel_pmt/crashlog1/"},
            {"/sys/class/intel_pmt/crashlog2", "../../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-4.4.auto/intel_pmt/crashlog2/"},
            {"/sys/class/intel_pmt/crashlog3", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-4.8.auto/intel_pmt/crashlog3/"},
            {"/sys/class/intel_pmt/crashlog4", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-4.8.auto/intel_pmt/crashlog4/"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem10", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-2.6.auto/intel_pmt/telem10/"},
            {"/sys/class/intel_pmt/telem11", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-2.6.auto/intel_pmt/telem11/"},
            {"/sys/class/intel_pmt/telem12", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-2.6.auto/intel_pmt/telem12/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
            {"/sys/class/intel_pmt/telem4", "./../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-2.2.auto/intel_pmt/telem4/"},
            {"/sys/class/intel_pmt/telem6", "./../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-2.2.auto/intel_pmt/telem6/"},
            {"/sys/class/intel_pmt/telem5", "./../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-2.2.auto/intel_pmt/telem5/"},
            {"/sys/class/intel_pmt/telem8", "./../devices/pci0000:98/0000:98:01.0/0000:99:00.0/0000:9a:02.0/0000:9d:00.1/intel-dvsec-2.5.auto/intel_pmt/telem8/"},
            {"/sys/class/intel_pmt/telem7", "./../devices/pci0000:98/0000:98:01.0/0000:99:00.0/0000:9a:02.0/0000:9d:00.1/intel-dvsec-2.5.auto/intel_pmt/telem7/"},
            {"/sys/class/intel_pmt/telem9", "./../devices/pci0000:98/0000:98:01.0/0000:99:00.0/0000:9a:02.0/0000:9d:00.1/intel-dvsec-2.5.auto/intel_pmt/telem9/"},

        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xfdc76195\n"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0xFEEDBEADDEABDEEF;
                memcpy(buf, &data, sizeof(data));
                return sizeof(data);
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    uint64_t expectedVal = 0xFEEDBEADDEABDEEF;

    std::array<uint8_t, 16> uuid;
    uint8_t expectedUuid[16] = {};
    std::memcpy(expectedUuid, &expectedVal, sizeof(expectedVal));
    EXPECT_EQ(true, HwInfoConfig::get(productFamily)->getUuid(devices[0].get(), uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        uint8_t expectedUuid[16] = {0};
        std::memcpy(expectedUuid, &expectedVal, sizeof(expectedVal));
        expectedUuid[15] = i + 1;
        EXPECT_EQ(true, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, whenRetrievingDeviceUuidThenCorrectUuidIsReceived, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/crashlog1", "../../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-4.4.auto/intel_pmt/crashlog1/"},
            {"/sys/class/intel_pmt/crashlog2", "../../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-4.4.auto/intel_pmt/crashlog2/"},
            {"/sys/class/intel_pmt/crashlog3", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-4.8.auto/intel_pmt/crashlog3/"},
            {"/sys/class/intel_pmt/crashlog4", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-4.8.auto/intel_pmt/crashlog4/"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem10", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-2.6.auto/intel_pmt/telem10/"},
            {"/sys/class/intel_pmt/telem11", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-2.6.auto/intel_pmt/telem11/"},
            {"/sys/class/intel_pmt/telem12", "../../devices/pci0000:e8/0000:e8:03.1/intel-dvsec-2.6.auto/intel_pmt/telem12/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
            {"/sys/class/intel_pmt/telem4", "./../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-2.2.auto/intel_pmt/telem4/"},
            {"/sys/class/intel_pmt/telem6", "./../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-2.2.auto/intel_pmt/telem6/"},
            {"/sys/class/intel_pmt/telem5", "./../devices/pci0000:6a/0000:6a:03.1/intel-dvsec-2.2.auto/intel_pmt/telem5/"},
            {"/sys/class/intel_pmt/telem8", "./../devices/pci0000:98/0000:98:01.0/0000:99:00.0/0000:9a:02.0/0000:9d:00.1/intel-dvsec-2.5.auto/intel_pmt/telem8/"},
            {"/sys/class/intel_pmt/telem7", "./../devices/pci0000:98/0000:98:01.0/0000:99:00.0/0000:9a:02.0/0000:9d:00.1/intel-dvsec-2.5.auto/intel_pmt/telem7/"},
            {"/sys/class/intel_pmt/telem9", "./../devices/pci0000:98/0000:98:01.0/0000:99:00.0/0000:9a:02.0/0000:9d:00.1/intel-dvsec-2.5.auto/intel_pmt/telem9/"},

        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        const std::string unSetGuid("0x41fe79a5");
        const uint32_t unSetBytesRead = 8;
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", unSetGuid},
            {"/sys/class/intel_pmt/telem1/offset", "0"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0xFEEDBEADDEABDEEF;
                memcpy(buf, &data, sizeof(data));
                if (unSetGuid == "0x41fe79a5") {
                    return unSetBytesRead;
                } else {
                    return sizeof(data);
                }
            }
            if (supportedFiles[fd].second == "dummy1") {
                uint64_t data = 0xABCDEFFEDCBA;
                memcpy(buf, &data, sizeof(data));
                if (unSetGuid == "0x41fe79a5") {
                    return unSetBytesRead;
                } else {
                    return sizeof(data);
                }
                return sizeof(data);
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    uint64_t expectedVal = 0xFEEDBEADDEABDEEF;

    std::array<uint8_t, 16> uuid;
    uint8_t expectedUuid[16] = {};
    std::memcpy(expectedUuid, &expectedVal, sizeof(expectedVal));
    EXPECT_EQ(true, HwInfoConfig::get(productFamily)->getUuid(devices[0].get(), uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        uint8_t expectedUuid[16] = {0};
        std::memcpy(expectedUuid, &expectedVal, sizeof(expectedVal));
        expectedUuid[15] = i + 1;
        EXPECT_EQ(true, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, givenTelemDirectoriesAreLessThanExpectedWhenRetrievingUuidForSubDeviceThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, GivenMissingGuidWhenRetrievingUuidForSubDeviceThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/offset", "0"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0xFEEDBEADDEABDEEF;
                memcpy(buf, &data, sizeof(data));
                return sizeof(data);
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, GivenIncorrectGuidWhenRetrievingUuidForSubDeviceThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, GivenMissingOffsetWhenRetrievingUuidForSubDeviceThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xfdc76195"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, GivenIncorrectOffsetWhenRetrievingUuidForSubDeviceThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xfdc76195"},
        };

        fd -= 1;
        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }
        return -1;
    });

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(subDevices[i], uuid));
    }
}

HWTEST2_F(MultipleDeviceUuidTest, GivenMissingTelemNodeWhenRetrievingUuidThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
        };
        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xfdc76195"},
            {"/sys/class/intel_pmt/telem1/offset", "0"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(devices[0].get(), uuid));
}

HWTEST2_F(MultipleDeviceUuidTest, GivenIncorrectTelemNodeWhenRetrievingUuidThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };
        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xfdc76195"},
            {"/sys/class/intel_pmt/telem1/offset", "0"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(devices[0].get(), uuid));
}

HWTEST2_F(MultipleDeviceUuidTest, GivenIncorrectGuidValueWhenRetrievingUuidThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };
        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xabcddcba"},
            {"/sys/class/intel_pmt/telem1/offset", "0"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(devices[0].get(), uuid));
}

HWTEST2_F(MultipleDeviceUuidTest, GivenDeviceLinkIsNotAvailableWhenRetrievingUuidForRootDeviceThenFailureIsReturned, IsXEHP) {

    VariableBackup<decltype(SysCalls::sysCallsReadlink)> mockReadLink(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });

    std::array<uint8_t, 16> uuid;
    EXPECT_EQ(false, HwInfoConfig::get(productFamily)->getUuid(devices[0].get(), uuid));
}

TEST(PmtUtilTest, givenDataPtrIsNullWhenPmtUtilReadTelemIsCalledThenVerifyZeroIsReturned) {

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return 16;
    });

    EXPECT_TRUE(0 == PmtUtil::readTelem("dummy", 16, 0, nullptr));
}

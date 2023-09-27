/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

struct MultipleDeviceUuidTest : public ::testing::Test {
    void SetUp() override {

        ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
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
    auto &productHelper = devices[0]->getProductHelper();
    EXPECT_EQ(true, productHelper.getUuid(devices[0].get()->getRootDeviceEnvironment().osInterface->getDriverModel(), 0u, 0u, uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));

    uint32_t subDeviceCount = numSubDevices;
    std::vector<SubDevice *> subDevices(subDeviceCount);
    subDevices = devices[0]->getSubDevices();
    for (auto i = 0u; i < subDeviceCount; i++) {
        std::array<uint8_t, 16> uuid;
        uint8_t expectedUuid[16] = {0};
        std::memcpy(expectedUuid, &expectedVal, sizeof(expectedVal));
        expectedUuid[15] = i + 1;
        EXPECT_EQ(true, productHelper.getUuid(devices[0].get()->getRootDeviceEnvironment().osInterface->getDriverModel(), subDeviceCount, i + 1, uuid));
        EXPECT_TRUE(0 == std::memcmp(uuid.data(), expectedUuid, sizeof(expectedUuid)));
    }
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

namespace NEO {
struct SysfsBasedUuidTest : public ::testing::Test {
    void SetUp() override {

        deviceFactory = std::make_unique<UltDeviceFactory>(1, 0);
        device = static_cast<Device *>(deviceFactory->rootDevices[0]);
        auto executionEnvironment = device->getExecutionEnvironment();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
        auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
    }

    void TearDown() override {}

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    Device *device = nullptr;
};
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

HWTEST2_F(SysfsBasedUuidTest, whenRetrievingDeviceUuidThenCorrectUuidIsReceived, IsDG2) {

    const auto baseFolder = std::string(Os::sysFsPciPathPrefix) + "/drm";
    NEO::directoryFilesMap.insert({baseFolder, {baseFolder + "/card0"}});
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string uuid("6769df256e271362\n");
        memcpy_s(buf, count, uuid.c_str(), uuid.size());
        return count;
    });

    // Prepare expected Uuid value
    const uint64_t expectedUuidValue = 0x6769df256e271362;
    std::array<uint8_t, 16> uuid;
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(true, productHelper.getUuid(device->getRootDeviceEnvironment().osInterface->getDriverModel(), 0u, 0u, uuid));
    EXPECT_TRUE(0 == std::memcmp(uuid.data(), &expectedUuidValue, sizeof(expectedUuidValue)));
    NEO::directoryFilesMap.clear();
}

HWTEST2_F(SysfsBasedUuidTest, givenSysfsFileNotAvailableWhenRetrievingDeviceUuidThenFailureIsReturned, IsDG2) {

    const auto baseFolder = std::string(Os::sysFsPciPathPrefix) + "/drm";
    NEO::directoryFilesMap.insert({baseFolder, {baseFolder + "/card0"}});
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    std::array<uint8_t, 16> uuid;
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(false, productHelper.getUuid(device->getRootDeviceEnvironment().osInterface->getDriverModel(), 0u, 0u, uuid));
    NEO::directoryFilesMap.clear();
}

HWTEST2_F(SysfsBasedUuidTest, givenIncorrectUuidWhenRetrievingDeviceUuidThenFailureIsReturned, IsDG2) {

    const auto baseFolder = std::string(Os::sysFsPciPathPrefix) + "/drm";
    NEO::directoryFilesMap.insert({baseFolder, {baseFolder + "/card0"}});
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string uuid("Z76Qdf256e271362\n");
        memcpy_s(buf, count, uuid.c_str(), uuid.size());
        return count;
    });

    std::array<uint8_t, 16> uuid;
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(false, productHelper.getUuid(device->getRootDeviceEnvironment().osInterface->getDriverModel(), 0u, 0u, uuid));
    NEO::directoryFilesMap.clear();
}

HWTEST2_F(SysfsBasedUuidTest, givenErrnoIsSetWhenRetrievingDeviceUuidThenFailureIsReturned, IsDG2) {

    const auto baseFolder = std::string(Os::sysFsPciPathPrefix) + "/drm";
    NEO::directoryFilesMap.insert({baseFolder, {baseFolder + "/card0"}});
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string uuid("6769df256e271362\n");
        memcpy_s(buf, count, uuid.c_str(), uuid.size());
        errno = 1;
        return count;
    });

    std::array<uint8_t, 16> uuid;
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(false, productHelper.getUuid(device->getRootDeviceEnvironment().osInterface->getDriverModel(), 0u, 0u, uuid));
    NEO::directoryFilesMap.clear();
}

HWTEST2_F(SysfsBasedUuidTest, givenDriverModelIsNotDrmWhenRetrievingDeviceUuidThenFailureIsReturned, IsDG2) {

    auto driverModelMock = std::make_unique<MockDriverModel>();
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(driverModelMock));
    std::array<uint8_t, 16> uuid;
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(false, productHelper.getUuid(device->getRootDeviceEnvironment().osInterface->getDriverModel(), 0u, 0u, uuid));
}
/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "mock_global_operations.h"

extern bool sysmanUltsEnable;

using ::testing::Matcher;

namespace L0 {
namespace ult {

constexpr uint64_t memSize1 = 2048;
constexpr uint64_t memSize2 = 1024;
constexpr uint64_t memSize4 = 1024;
constexpr uint64_t memSize6 = 1024;
constexpr uint64_t memSize7 = 0;
constexpr uint64_t sharedMemSize1 = 1024;
constexpr uint64_t sharedMemSize2 = 512;
constexpr uint64_t sharedMemSize4 = 512;
constexpr uint64_t sharedMemSize6 = 512;
constexpr uint64_t sharedMemSize7 = 0;
// In mock function getValUnsignedLong, we have set the engines used as 0, 3 and 1.
// Hence, expecting 28 as engine field because 28 in binary would be 00011100
// This indicates bit number 2, 3 and 4 are set, thus this indicates, this process
// used ZES_ENGINE_TYPE_FLAG_3D, ZES_ENGINE_TYPE_FLAG_MEDIA and ZES_ENGINE_TYPE_FLAG_DMA
// Their corresponding mapping with i915 engine numbers are 0, 3 and 1 respectively.
constexpr int64_t engines1 = 28u;
// 4 in binary 0100, as 2nd bit is set, hence it indicates, process used ZES_ENGINE_TYPE_FLAG_3D
// Corresponding i915 mapped value in mocked getValUnsignedLong() is 0.
constexpr int64_t engines2 = 4u;
constexpr int64_t engines4 = 20u;
constexpr int64_t engines6 = 1u;
constexpr int64_t engines7 = 1u;
constexpr uint32_t totalProcessStates = 5u; // Three process States for three pids
constexpr uint32_t totalProcessStatesForFaultyClients = 3u;
class SysmanGlobalOperationsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<GlobalOperationsEngineHandleContext>> pEngineHandleContext;
    std::unique_ptr<Mock<GlobalOperationsDiagnosticsHandleContext>> pDiagnosticsHandleContext;
    std::unique_ptr<Mock<GlobalOperationsFirmwareHandleContext>> pFirmwareHandleContext;
    std::unique_ptr<Mock<GlobalOperationsRasHandleContext>> pRasHandleContext;
    std::unique_ptr<Mock<GlobalOperationsSysfsAccess>> pSysfsAccess;
    std::unique_ptr<Mock<GlobalOperationsProcfsAccess>> pProcfsAccess;
    std::unique_ptr<Mock<GlobalOperationsFsAccess>> pFsAccess;
    std::unique_ptr<MockGlobalOpsLinuxSysmanImp> pMockGlobalOpsLinuxSysmanImp;
    EngineHandleContext *pEngineHandleContextOld = nullptr;
    DiagnosticsHandleContext *pDiagnosticsHandleContextOld = nullptr;
    FirmwareHandleContext *pFirmwareHandleContextOld = nullptr;
    RasHandleContext *pRasHandleContextOld = nullptr;
    SysfsAccess *pSysfsAccessOld = nullptr;
    ProcfsAccess *pProcfsAccessOld = nullptr;
    FsAccess *pFsAccessOld = nullptr;
    LinuxSysmanImp *pLinuxSysmanImpOld = nullptr;
    OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::GlobalOperationsImp *pGlobalOperationsImp;
    std::string expectedModelName;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pEngineHandleContextOld = pSysmanDeviceImp->pEngineHandleContext;
        pDiagnosticsHandleContextOld = pSysmanDeviceImp->pDiagnosticsHandleContext;
        pFirmwareHandleContextOld = pSysmanDeviceImp->pFirmwareHandleContext;
        pRasHandleContextOld = pSysmanDeviceImp->pRasHandleContext;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pProcfsAccessOld = pLinuxSysmanImp->pProcfsAccess;
        pFsAccessOld = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImpOld = pLinuxSysmanImp;

        pEngineHandleContext = std::make_unique<NiceMock<Mock<GlobalOperationsEngineHandleContext>>>(pOsSysman);
        pSysfsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsSysfsAccess>>>();
        pProcfsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsProcfsAccess>>>();
        pFsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsFsAccess>>>();
        pDiagnosticsHandleContext = std::make_unique<NiceMock<Mock<GlobalOperationsDiagnosticsHandleContext>>>(pOsSysman);
        pFirmwareHandleContext = std::make_unique<NiceMock<Mock<GlobalOperationsFirmwareHandleContext>>>(pOsSysman);
        pRasHandleContext = std::make_unique<NiceMock<Mock<GlobalOperationsRasHandleContext>>>(pOsSysman);
        pMockGlobalOpsLinuxSysmanImp = std::make_unique<MockGlobalOpsLinuxSysmanImp>(pLinuxSysmanImp->getSysmanDeviceImp());

        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContext.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysmanDeviceImp->pDiagnosticsHandleContext = pDiagnosticsHandleContext.get();
        pSysmanDeviceImp->pFirmwareHandleContext = pFirmwareHandleContext.get();
        pSysmanDeviceImp->pRasHandleContext = pRasHandleContext.get();

        ON_CALL(*pRasHandleContext.get(), init(_))
            .WillByDefault(::testing::Invoke(pRasHandleContext.get(), &Mock<GlobalOperationsRasHandleContext>::initMock));
        ON_CALL(*pEngineHandleContext.get(), init())
            .WillByDefault(::testing::Invoke(pEngineHandleContext.get(), &Mock<GlobalOperationsEngineHandleContext>::initMock));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDirEntries));
        ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValAgamaFile));

        ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
        ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
            .WillByDefault(::testing::Return(ZE_RESULT_SUCCESS));
        ON_CALL(*pProcfsAccess.get(), myProcessId())
            .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
        ON_CALL(*pProcfsAccess.get(), isAlive(_))
            .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
        ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
        ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
        ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
            .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
        ON_CALL(*pSysfsAccess.get(), unbindDevice(_))
            .WillByDefault(::testing::Return(ZE_RESULT_SUCCESS));
        ON_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
            .WillByDefault(::testing::Return(ZE_RESULT_SUCCESS));

        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        expectedModelName = neoDevice->getDeviceName(neoDevice->getHardwareInfo());
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        if (nullptr != pGlobalOperationsImp->pOsGlobalOperations) {
            delete pGlobalOperationsImp->pOsGlobalOperations;
        }
        pGlobalOperationsImp->pOsGlobalOperations = pOsGlobalOperationsPrev;
        pGlobalOperationsImp = nullptr;
        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContextOld;
        pSysmanDeviceImp->pDiagnosticsHandleContext = pDiagnosticsHandleContextOld;
        pSysmanDeviceImp->pFirmwareHandleContext = pFirmwareHandleContextOld;
        pSysmanDeviceImp->pRasHandleContext = pRasHandleContextOld;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccessOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOld;

        SysmanDeviceFixture::TearDown();
    }
    void initGlobalOps() {
        zes_device_state_t deviceState;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    }
};
class SysmanGlobalOperationsIntegratedFixture : public SysmanGlobalOperationsFixture {
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanGlobalOperationsFixture::SetUp();
        auto mockHardwareInfo = neoDevice->getHardwareInfo();
        mockHardwareInfo.capabilityTable.isIntegratedDevice = true;
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfo(&mockHardwareInfo);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanGlobalOperationsFixture::TearDown();
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesGlobalOperationsGetPropertiesThenVerifyValidPropertiesAreReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
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
            {"/sys/class/intel_pmt/telem1/guid", "0x41fe79a5\n"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        if ((--fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0x3e8c9dfe1c2e4d5c;
                memcpy(buf, &data, sizeof(data));
                return count;
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return count;
        }

        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    const std::string expectedSerialNumber("0x3e8c9dfe1c2e4d5c");
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.numSubdevices, 0u);
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.brandName));
    EXPECT_TRUE(0 == driverVersion.compare(properties.driverVersion));
    EXPECT_TRUE(0 == expectedModelName.compare(properties.modelName));
    EXPECT_TRUE(0 == expectedSerialNumber.compare(properties.serialNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.vendorName));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndReadTelemOffsetFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            if (std::string(pathname) == "/sys/class/intel_pmt/telem1/offset") {
                return 0;
            }
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0x41fe79a5\n"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };
        if ((--fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0x3e8c9dfe1c2e4d5c;
                memcpy(buf, &data, sizeof(data));
                return count;
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return count;
        }

        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndInvalidGuidWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "Invalidguid\n"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        if ((--fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0x3e8c9dfe1c2e4d5c;
                memcpy(buf, &data, sizeof(data));
                return count;
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return count;
        }

        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndPpinOffsetIsAbsentWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xb15a0edd\n"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        if ((--fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0x3e8c9dfe1c2e4d5c;
                memcpy(buf, &data, sizeof(data));
                return count;
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return count;
        }

        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndReadPpinInfoFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0x41fe79a5\n"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        if ((--fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].first == "/sys/class/intel_pmt/telem1/telem") {
                return 0;
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return count;
        }

        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndOpenSysCallFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 0;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndTelemNodeReadLinkFailsWhenCallingzesGlobalOperationsGetPropertiesThenVerifyInvalidSerialNumberIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8a:00.0/drm/renderD128"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndReadLinkFailsWhenCallingzesGlobalOperationsGetPropertiesThenVerifyInvalidSerialNumberIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    auto pDrmMock = std::make_unique<DrmMock>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    auto pOriginalDrm = pLinuxSysmanImp->pDrm;
    pLinuxSysmanImp->pDrm = pDrmMock.get();

    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAgmaFileIsAbsentThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    std::string test;
    test = srcVersion;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValSrcFile));

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == test.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAgmaFileAndSrcFileIsAbsentThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;

    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_NOT_AVAILABLE));

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileIsNotAvaliableThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_NOT_AVAILABLE));

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileReadFailsThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDevicePropertiesWhenVendorIsUnKnownThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getFalseValString));
    neoDevice->deviceInfo.vendorId = 1806; // Unknown Vendor id
    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.vendorName));
    EXPECT_TRUE(0 == unknown.compare(properties.brandName));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAccessingAgamaFileOrSrcFileGotPermissionDeniedThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS));

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenSuccessIsReturned) {
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceProcessesGetState(device, &count, nullptr));
    EXPECT_EQ(count, totalProcessStates);
    std::vector<zes_process_state_t> processes(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceProcessesGetState(device, &count, processes.data()));
    EXPECT_EQ(processes[0].processId, pid1);
    EXPECT_EQ(processes[0].engines, engines1);
    EXPECT_EQ(processes[0].memSize, memSize1);
    EXPECT_EQ(processes[0].sharedSize, sharedMemSize1);
    EXPECT_EQ(processes[1].processId, pid2);
    EXPECT_EQ(processes[1].engines, engines2);
    EXPECT_EQ(processes[1].memSize, memSize2);
    EXPECT_EQ(processes[1].sharedSize, sharedMemSize2);
    EXPECT_EQ(processes[2].processId, pid4);
    EXPECT_EQ(processes[2].engines, engines4);
    EXPECT_EQ(processes[2].memSize, memSize4);
    EXPECT_EQ(processes[2].sharedSize, sharedMemSize4);
    EXPECT_EQ(processes[3].processId, pid6);
    EXPECT_EQ(processes[3].engines, engines6);
    EXPECT_EQ(processes[3].memSize, memSize6);
    EXPECT_EQ(processes[3].sharedSize, sharedMemSize6);
    EXPECT_EQ(processes[4].processId, pid7);
    EXPECT_EQ(processes[4].engines, engines7);
    EXPECT_EQ(processes[4].memSize, memSize7);
    EXPECT_EQ(processes[4].sharedSize, sharedMemSize7);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenSuccessIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDir4Entries));
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValUnsignedLongCreatedBytesSuccess));

    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceProcessesGetState(device, &count, nullptr));
    EXPECT_EQ(count, totalProcessStatesForFaultyClients);
    std::vector<zes_process_state_t> processes(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceProcessesGetState(device, &count, processes.data()));
    EXPECT_EQ(processes[0].processId, pid1);
    EXPECT_EQ(processes[0].engines, engines1);
    EXPECT_EQ(processes[0].memSize, memSize1);
    EXPECT_EQ(processes[0].sharedSize, sharedMemSize1);
    EXPECT_EQ(processes[1].processId, pid2);
    EXPECT_EQ(processes[1].engines, engines2);
    EXPECT_EQ(processes[1].memSize, memSize2);
    EXPECT_EQ(processes[1].sharedSize, sharedMemSize2);
    EXPECT_EQ(processes[2].processId, pid4);
    EXPECT_EQ(processes[2].engines, engines4);
    EXPECT_EQ(processes[2].memSize, memSize4);
    EXPECT_EQ(processes[2].sharedSize, sharedMemSize4);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileCountValueIsProvidedThenFailureIsReturned) {
    uint32_t count = 2;

    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingFaultyClientFileThenFailureIsReturned) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDir4Entries));
    ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingNullDirThenFailureIsReturned) {
    uint32_t count = 0;
    EXPECT_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceProcessesGetState(device, &count, nullptr));
}
TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenFailureIsReturnedEvenwithFaultyClient) {
    initGlobalOps();
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDirPidEntires));
    EXPECT_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    EXPECT_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingBusyDirForEnginesReadThenFailureIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDirPidEntires));

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingBusyDirForEnginesThenFailureIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDir4Entries));
    EXPECT_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValUnsignedLongCreatedBytesSuccess))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingInvalidBufferObjectsThenErrorIsReturned) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDirPidEntiresForClients));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingExistingMemoryFileThenCorrectValueIsReturned) {
    uint64_t memSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysfsAccess->read("clients/6/total_device_memory_buffer_objects/created_bytes", memSize));
    EXPECT_EQ(memSize2, memSize);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingInvalidMemoryFileThenErrorIsReturned) {
    uint64_t memSize = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDir4Entries));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysfsAccess->read("clients/7/total_device_memory_buffer_objects/imported_bytes", memSize));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingNonExistingFileThenErrorIsReturned) {
    std::vector<std::string> engineEntries;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysfsAccess->scanDirEntries("clients/7/busy", engineEntries));
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceIsWedgedWhenCallingGetDeviceStateThenZesResetReasonFlagWedgedIsReturned) {
    ON_CALL(*pFsAccess.get(), read(_, Matcher<uint32_t &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValWedgedFileTrue));
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(ZES_RESET_REASON_FLAG_WEDGED, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceIsNotWedgedWhenCallingGetDeviceStateThenZeroIsReturned) {
    ON_CALL(*pFsAccess.get(), read(_, Matcher<uint32_t &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValWedgedFileFalse));
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueWhenCallingResetThenSuccessIsReturned) {
    initGlobalOps();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenPermissionDeniedWhenCallingGetDeviceStateThenZeResultErrorInsufficientPermissionsIsReturned) {

    pSysfsAccess->isRootSet = false;
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceInUseWhenCallingResetThenZeResultErrorHandleObjectInUseIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceNotInUseWhenCallingResetThenSuccessIsReturned) {

    // Pretend we have the device open
    pProcfsAccess->ourDevicePid = getpid();
    pProcfsAccess->ourDeviceFd = ::open("/dev/null", 0);

    // The first time we get the process list, include our own process, that has the file open
    // Reset should close the file (we verify after reset). On subsequent calls, return
    // the process list without our process
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_SUCCESS));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // Check that reset closed the device
    // If the device is already closed, then close will fail with errno of EBADF
    EXPECT_NE(0, ::close(pProcfsAccess->ourDevicePid));
    EXPECT_EQ(errno, EBADF);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenForceTrueAndDeviceInUseWhenCallingResetThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_SUCCESS));
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_SUCCESS));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndIfNeoDeviceCreateFailsThenErrorIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_SUCCESS));
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.mockedPrepareDeviceEnvironmentsFuncResult = false;
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndBindFailsThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceInUseWhenCallingResetAndListProcessesFailsThenZeResultErrorIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenListProcessesFailsAfterUnbindThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    // Return process list without open fd on first call
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    ON_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndWriteFailsAfterUnbindThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndUnbindFailsThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused));
    ON_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndGetFileNameFailsThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    ON_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_SUCCESS));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}
TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessWontDieWhenCallingResetThenZeResultErrorHandleObjectInUseErrorIsReturned) {
    initGlobalOps();
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate

    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .Times(1);
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .Times(0);
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .Times(0);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndGetFileDescriptorsFailsThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    EXPECT_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptorsFailure))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    ON_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Return(ZE_RESULT_SUCCESS));
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(SysmanGlobalOperationsTest, GivenValidDevicePciPathWhenPreparingDeviceEnvironmentThenPrepareDeviceEnvironmentReturnsTrue) {
    auto device1 = std::unique_ptr<MockDevice>{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
    std::string pciPath1 = "0000:00:02.0";
    EXPECT_TRUE(DeviceFactory::prepareDeviceEnvironment(*device1->getExecutionEnvironment(), pciPath1, 0u));
}

TEST(SysmanGlobalOperationsTest, GivenValidDevicePciPathWhoseFileDescriptorOpenFailedThenPrepareDeviceEnvironmentReturnsFalse) {
    auto device2 = std::unique_ptr<MockDevice>{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
    std::string pciPath2 = "0000:00:03.0";
    EXPECT_FALSE(DeviceFactory::prepareDeviceEnvironment(*device2->getExecutionEnvironment(), pciPath2, 0u));
}

TEST(SysmanGlobalOperationsTest, GivenNotExisitingPciPathWhenPrepareDeviceEnvironmentIsCalledThenFalseIsReturned) {
    auto device3 = std::unique_ptr<MockDevice>{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
    std::string pciPath3 = "0000:00:04.0";
    EXPECT_FALSE(DeviceFactory::prepareDeviceEnvironment(*device3->getExecutionEnvironment(), pciPath3, 0u));
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenCallingDeviceGetStateThenSuccessResultIsReturned) {
    zes_device_state_t deviceState;
    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0

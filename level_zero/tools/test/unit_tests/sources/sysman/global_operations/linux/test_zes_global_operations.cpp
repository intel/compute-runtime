/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_global_operations.h"

extern bool sysmanUltsEnable;

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
    std::unique_ptr<MockGlobalOperationsEngineHandleContext> pEngineHandleContext;
    std::unique_ptr<MockGlobalOperationsDiagnosticsHandleContext> pDiagnosticsHandleContext;
    std::unique_ptr<MockGlobalOperationsFirmwareHandleContext> pFirmwareHandleContext;
    std::unique_ptr<MockGlobalOperationsRasHandleContext> pRasHandleContext;
    std::unique_ptr<MockGlobalOperationsSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockGlobalOperationsProcfsAccess> pProcfsAccess;
    std::unique_ptr<MockGlobalOperationsFsAccess> pFsAccess;
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

        pEngineHandleContext = std::make_unique<MockGlobalOperationsEngineHandleContext>(pOsSysman);
        pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
        pProcfsAccess = std::make_unique<MockGlobalOperationsProcfsAccess>();
        pFsAccess = std::make_unique<MockGlobalOperationsFsAccess>();
        pDiagnosticsHandleContext = std::make_unique<MockGlobalOperationsDiagnosticsHandleContext>(pOsSysman);
        pFirmwareHandleContext = std::make_unique<MockGlobalOperationsFirmwareHandleContext>(pOsSysman);
        pRasHandleContext = std::make_unique<MockGlobalOperationsRasHandleContext>(pOsSysman);
        pMockGlobalOpsLinuxSysmanImp = std::make_unique<MockGlobalOpsLinuxSysmanImp>(pLinuxSysmanImp->getSysmanDeviceImp());

        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContext.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysmanDeviceImp->pDiagnosticsHandleContext = pDiagnosticsHandleContext.get();
        pSysmanDeviceImp->pFirmwareHandleContext = pFirmwareHandleContext.get();
        pSysmanDeviceImp->pRasHandleContext = pRasHandleContext.get();
        pSysfsAccess->mockReadVal = "0x8086";
        pFsAccess->mockReadVal = driverVersion;
        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        expectedModelName = neoDevice->getDeviceName();
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
        auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        auto pDrm = std::make_unique<DrmGlobalOpsMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        VariableBackup<NEO::Drm *> backup(&pLinuxSysmanImp->pDrm);
        pLinuxSysmanImp->pDrm = pDrm.get();
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
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanGlobalOperationsFixture::TearDown();
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingzesDeviceResetExtThenResetExtCallReturnSuccess) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    initGlobalOps();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_WARM};
    ze_result_t result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pProperties.resetType = ZES_RESET_TYPE_COLD;
    result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pProperties.resetType = ZES_RESET_TYPE_FLR;
    result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingZesDeviceResetExtForColdResetThenErrorIsReturned) {
    initGlobalOps();
    pProcfsAccess->ourDevicePid = pProcfsAccess->pidList[0];
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate
    pMockGlobalOpsLinuxSysmanImp->ourDevicePid = pProcfsAccess->ourDevicePid;
    pMockGlobalOpsLinuxSysmanImp->ourDeviceFd = pProcfsAccess->ourDevicePid;
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockNoKill = true;
    pSysfsAccess->mockBindDeviceError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_COLD};
    ze_result_t result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingResetExtWithInvalidTypeThenFailureIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    initGlobalOps();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_FORCE_UINT32};
    ze_result_t result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenGettingSysfsPathFailsWhenCallingResetExtThenFailureIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    initGlobalOps();
    pSysfsAccess->mockDeviceUnbound = true;
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_FORCE_UINT32};
    ze_result_t result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

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
                if (count == sizeof(uint64_t)) {
                    uint64_t data = 0x3e8c9dfe1c2e4d5c;
                    memcpy(buf, &data, sizeof(data));
                    return count;
                } else {
                    // Board number will be in ASCII format, Expected board number should be decoded value
                    // i.e 0821VPTW910091000821VPTW91009100 for data provided below.
                    uint64_t data[] = {0x5754505631323830, 0x3030313930303139, 0x5754505631323830, 0x3030313930303139};
                    memcpy(buf, &data, sizeof(data));
                    return count;
                }
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
    const std::string expectedBoardNumber("1280WTPV001900191280WTPV00190019");
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.numSubdevices, 0u);
    EXPECT_TRUE(0 == expectedBoardNumber.compare(properties.boardNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.brandName));
    EXPECT_TRUE(0 == driverVersion.compare(properties.driverVersion));
    EXPECT_TRUE(0 == expectedModelName.compare(properties.modelName));
    EXPECT_TRUE(0 == expectedSerialNumber.compare(properties.serialNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.vendorName));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndReadTelemOffsetFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndInvalidGuidWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {
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
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndPpinandBoardNumberOffsetAreAbsentWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndReadTelemDataFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndOpenSysCallFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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

    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    properties.pNext = nullptr;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndTelemNodeReadLinkFailsWhenCallingzesGlobalOperationsGetPropertiesThenVerifyInvalidSerialNumberAndBoardNumberAreReturned) {

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
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleAndReadLinkFailsWhenCallingzesGlobalOperationsGetPropertiesThenVerifyInvalidSerialNumberAndBoardNumberAreReturned) {

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
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    pLinuxSysmanImp->pDrm = pOriginalDrm;
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAgmaFileIsAbsentThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    std::string test;
    test = srcVersion;
    pFsAccess->mockReadVal = srcVersion;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == test.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAgmaFileAndSrcFileIsAbsentThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    pFsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileIsNotAvaliableThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    pFsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileReadFailsThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    pFsAccess->mockReadError = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDevicePropertiesWhenVendorIsUnKnownThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    pSysfsAccess->mockReadVal = "0xa086";
    neoDevice->deviceInfo.vendorId = 1806; // Unknown Vendor id
    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.vendorName));
    EXPECT_TRUE(0 == unknown.compare(properties.brandName));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAccessingAgamaFileOrSrcFileGotPermissionDeniedThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    pFsAccess->mockReadError = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
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
    pSysfsAccess->mockGetScannedDir4EntriesStatus = true;
    pSysfsAccess->mockGetValUnsignedLongStatus = true;
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
    pSysfsAccess->mockGetScannedDir4EntriesStatus = true;
    ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingNullDirThenFailureIsReturned) {
    uint32_t count = 0;
    pSysfsAccess->mockScanDirEntriesError = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceProcessesGetState(device, &count, nullptr));
}
TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenFailureIsReturnedEvenwithFaultyClient) {
    initGlobalOps();
    uint32_t count = 0;
    pSysfsAccess->mockGetScannedDirPidEntriesStatus = true;
    pSysfsAccess->mockReadError = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingBusyDirForEnginesReadThenFailureIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    pSysfsAccess->mockGetScannedDirPidEntriesStatus = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingBusyDirForEnginesThenFailureIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    pSysfsAccess->mockGetScannedDir4EntriesStatus = true;
    pSysfsAccess->mockReadStatus = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingInvalidBufferObjectsThenErrorIsReturned) {
    uint32_t count = 0;
    pSysfsAccess->mockGetScannedDirPidEntriesForClientsStatus = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingExistingMemoryFileThenCorrectValueIsReturned) {
    uint64_t memSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysfsAccess->read("clients/6/total_device_memory_buffer_objects/created_bytes", memSize));
    EXPECT_EQ(memSize2, memSize);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingInvalidMemoryFileThenErrorIsReturned) {
    uint64_t memSize = 0;
    pSysfsAccess->mockGetScannedDir4EntriesStatus = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysfsAccess->read("clients/7/total_device_memory_buffer_objects/imported_bytes", memSize));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileReadingNonExistingFileThenErrorIsReturned) {
    std::vector<std::string> engineEntries;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysfsAccess->scanDirEntries("clients/7/busy", engineEntries));
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceIsWedgedWhenCallingGetDeviceStateThenZesResetReasonFlagWedgedIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = std::make_unique<DrmGlobalOpsMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->ioctlRetVal = -1;
    pDrm->ioctlErrno = EIO;
    VariableBackup<NEO::Drm *> backup(&pLinuxSysmanImp->pDrm);
    pLinuxSysmanImp->pDrm = pDrm.get();
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(ZES_RESET_REASON_FLAG_WEDGED, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceIsNotWedgedWhenCallingGetDeviceStateThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = std::make_unique<DrmGlobalOpsMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    VariableBackup<NEO::Drm *> backup(&pLinuxSysmanImp->pDrm);
    pLinuxSysmanImp->pDrm = pDrm.get();
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenGemCreateIoctlFailsWithEINVALWhenCallingGetDeviceStateThenVerifyResetIsNotNeeded) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = std::make_unique<DrmGlobalOpsMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->ioctlRetVal = -1;
    pDrm->ioctlErrno = EINVAL;
    VariableBackup<NEO::Drm *> backup(&pLinuxSysmanImp->pDrm);
    pLinuxSysmanImp->pDrm = pDrm.get();
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueWhenCallingResetThenSuccessIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    initGlobalOps();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenPermissionDeniedWhenCallingGetDeviceStateThenZeResultErrorInsufficientPermissionsIsReturned) {
    pSysfsAccess->isRootSet = false;
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingResetThenZeResultErrorHandleObjectInUseIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenPermissionDeniedWhenCallingGetDeviceStateThenZeResultErrorInsufficientPermissionsIsReturned) {
    pSysfsAccess->isRootSet = false;
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceInUseWhenCallingResetThenZeResultErrorHandleObjectInUseIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceInUseAndBindingFailsDuringResetWhenCallingResetThenErrorIsReturned) {

    initGlobalOps();
    pProcfsAccess->ourDevicePid = pProcfsAccess->pidList[0]; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate

    pMockGlobalOpsLinuxSysmanImp->ourDevicePid = pProcfsAccess->ourDevicePid;
    pMockGlobalOpsLinuxSysmanImp->ourDeviceFd = pProcfsAccess->ourDevicePid;

    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockNoKill = true;
    pSysfsAccess->mockBindDeviceError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseAndReInitFailsDuringResetWhenCallingResetThenErrorIsReturned) {

    initGlobalOps();
    pProcfsAccess->ourDevicePid = pProcfsAccess->pidList[0]; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate

    pMockGlobalOpsLinuxSysmanImp->ourDevicePid = pProcfsAccess->ourDevicePid;
    pMockGlobalOpsLinuxSysmanImp->ourDeviceFd = pProcfsAccess->ourDevicePid;
    pMockGlobalOpsLinuxSysmanImp->setMockInitDeviceError(ZE_RESULT_ERROR_UNKNOWN);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockNoKill = true;
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceNotInUseWhenCallingResetThenSuccessIsReturned) {

    // Pretend we have the device open
    pProcfsAccess->ourDevicePid = getpid();
    pProcfsAccess->ourDeviceFd = ::open("/dev/null", 0);

    // The first time we get the process list, include our own process, that has the file open
    // Reset should close the file (we verify after reset). On subsequent calls, return
    // the process list without our process
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(true);
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
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndIfNeoDeviceCreateFailsThenErrorIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
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
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pSysfsAccess->mockBindDeviceError = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenDeviceInUseWhenCallingResetAndListProcessesFailsThenZeResultErrorIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    pProcfsAccess->mockListProcessCall.push_back(RETURN_ERROR);
    pProcfsAccess->isRepeated.push_back(false);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenListProcessesFailsAfterUnbindThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(RETURN_ERROR);
    pProcfsAccess->isRepeated.push_back(false);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndWriteFailsAfterUnbindThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pFsAccess->mockWriteError = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndUnbindFailsThenFailureIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(true);
    pSysfsAccess->mockUnbindDeviceError = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndGetFileNameFailsThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockGetFileNameError = ZE_RESULT_ERROR_UNKNOWN;
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
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcfsAccess->isRepeated.push_back(false);
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockNoKill = true;
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture, GivenProcessStartsMidResetWhenCallingResetAndGetFileDescriptorsFailsThenSuccessIsReturned) {

    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    // Return process list without open fd on first call, but with open fd on subsequent calls
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockGetFileDescriptorsError = ZE_RESULT_ERROR_UNKNOWN;
    pProcfsAccess->mockGetFileNameError = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(SysmanGlobalOperationsTest, GivenValidDevicePciPathWhenPreparingDeviceEnvironmentThenPrepareDeviceEnvironmentReturnsTrue) {
    MockExecutionEnvironment executionEnvironment;
    std::string pciPath1 = "0000:00:02.0";
    EXPECT_TRUE(DeviceFactory::prepareDeviceEnvironment(executionEnvironment, pciPath1, 0u));
}

TEST(SysmanGlobalOperationsTest, GivenValidDevicePciPathWhoseFileDescriptorOpenFailedThenPrepareDeviceEnvironmentReturnsFalse) {
    MockExecutionEnvironment executionEnvironment;
    std::string pciPath2 = "0000:00:03.0";
    EXPECT_FALSE(DeviceFactory::prepareDeviceEnvironment(executionEnvironment, pciPath2, 0u));
}

TEST(SysmanGlobalOperationsTest, GivenNotExisitingPciPathWhenPrepareDeviceEnvironmentIsCalledThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    std::string pciPath3 = "0000:00:04.0";
    EXPECT_FALSE(DeviceFactory::prepareDeviceEnvironment(executionEnvironment, pciPath3, 0u));
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenCallingDeviceGetStateThenSuccessResultIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = std::make_unique<DrmGlobalOpsMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    VariableBackup<NEO::Drm *> backup(&pLinuxSysmanImp->pDrm);
    pLinuxSysmanImp->pDrm = pDrm.get();
    zes_device_state_t deviceState;
    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/global_operations/linux/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"

#include <fcntl.h>
#include <sys/stat.h>

namespace L0 {
namespace Sysman {
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
    L0::Sysman::EngineHandleContext *pEngineHandleContextOld = nullptr;
    L0::Sysman::DiagnosticsHandleContext *pDiagnosticsHandleContextOld = nullptr;
    L0::Sysman::FirmwareHandleContext *pFirmwareHandleContextOld = nullptr;
    L0::Sysman::RasHandleContext *pRasHandleContextOld = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;
    L0::Sysman::ProcFsAccessInterface *pProcfsAccessOld = nullptr;
    L0::Sysman::FsAccessInterface *pFsAccessOld = nullptr;
    L0::Sysman::LinuxSysmanImp *pLinuxSysmanImpOld = nullptr;
    L0::Sysman::OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;
    L0::Sysman::SysmanDeviceImp *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
        pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();

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

        auto pDrmLocal = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrmLocal->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterfaceLocal = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterfaceLocal->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrmLocal));

        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContext.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysmanDeviceImp->pDiagnosticsHandleContext = pDiagnosticsHandleContext.get();
        pSysmanDeviceImp->pFirmwareHandleContext = pFirmwareHandleContext.get();
        pSysmanDeviceImp->pRasHandleContext = pRasHandleContext.get();
        pFsAccess->mockReadVal = driverVersion;
        pGlobalOperationsImp = static_cast<L0::Sysman::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
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
        SysmanGlobalOperationsFixture::SetUp();
        auto mockHardwareInfo = device->getHardwareInfo();
        // auto mockHardwareInfo = neoDevice->getHardwareInfo();
        mockHardwareInfo.capabilityTable.isIntegratedDevice = true;
        device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    }

    void TearDown() override {
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

    struct MockSysmanProductHelperGlobalOperation : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperGlobalOperation() = default;
        std::unique_ptr<std::map<std::string, std::map<std::string, uint64_t>>> mockMap;
        std::map<std::string, std::map<std::string, uint64_t>> *getGuidToKeyOffsetMap() override {
            mockMap = std::make_unique<std::map<std::string, std::map<std::string, uint64_t>>>();
            (*mockMap)["0x41fe79a5"] = {{"PPIN", 152},
                                        {"BoardNumber", 72}};
            return mockMap.get();
        }
    };
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperGlobalOperation>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    MockGlobalOperationsFsAccess *pFsAccess = new MockGlobalOperationsFsAccess();
    MockGlobalOperationsSysfsAccess *pSysfsAccess = new MockGlobalOperationsSysfsAccess();
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
    pFsAccess->mockReadVal = driverVersion;

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    const std::string expectedSerialNumber("0x3e8c9dfe1c2e4d5c");
    const std::string expectedBoardNumber("1280WTPV001900191280WTPV00190019");
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.numSubdevices, 0u);
    EXPECT_TRUE(0 == expectedBoardNumber.compare(properties.boardNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.brandName));
    EXPECT_TRUE(0 == driverVersion.compare(properties.driverVersion));

    std::stringstream expectedModelName;
    expectedModelName << "Intel(R) Graphics";
    expectedModelName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << device->getHardwareInfo().platform.usDeviceID << "]";

    EXPECT_TRUE(0 == expectedModelName.str().compare(properties.modelName));
    EXPECT_TRUE(0 == expectedSerialNumber.compare(properties.serialNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.vendorName));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndReadTelemOffsetFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndInvalidGuidWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {
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

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndPpinandBoardNumberOffsetAreAbsentWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndReadTelemDataFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndOpenGuidReadFailsWhenCallingzesGlobalOperationsGetPropertiesThenInvalidSerialNumberAndBoardNumberAreReturned) {

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
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return count;
        }
        return -1;
    });

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndTelemNodeReadLinkFailsWhenCallingzesGlobalOperationsGetPropertiesThenVerifyInvalidSerialNumberAndBoardNumberAreReturned) {

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

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleAndReadLinkFailsWhenCallingzesGlobalOperationsGetPropertiesThenVerifyInvalidSerialNumberAndBoardNumberAreReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    pLinuxSysmanImp->rootPath = NEO::getPciRootPath(pLinuxSysmanImp->getDrm()->getFileDescriptor()).value_or("");
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenAgamaFileIsAbsentWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionThenZesDeviceGetPropertiesCallSucceedsAndUnknownDriverVersionIsReturned) {

    MockGlobalOperationsFsAccess *pFsAccess = new MockGlobalOperationsFsAccess();
    MockGlobalOperationsSysfsAccess *pSysfsAccess = new MockGlobalOperationsSysfsAccess();
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    pFsAccess->mockReadVal = "";
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenAgamaFileIsPresentWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionThenVerifyzesDeviceGetPropertiesCallSucceedsAndDriverVersionIsReturned) {

    MockGlobalOperationsFsAccess *pFsAccess = new MockGlobalOperationsFsAccess();
    MockGlobalOperationsSysfsAccess *pSysfsAccess = new MockGlobalOperationsSysfsAccess();
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
    pFsAccess->mockReadVal = driverVersion;
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == driverVersion.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenSrcVersionFileIsAbsentUpstreamKernelWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionThenZesDeviceGetPropertiesCallSucceedsAndUnknownDriverVersionIsReturned) {

    MockGlobalOperationsFsAccess *pFsAccess = new MockGlobalOperationsFsAccess();
    MockGlobalOperationsSysfsAccess *pSysfsAccess = new MockGlobalOperationsSysfsAccess();
    MockSysmanKmdInterfaceUpstream *pSysmanKmdInterface = new MockSysmanKmdInterfaceUpstream(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    pFsAccess->mockReadVal = "";
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenSrcVersionFileIsPresentAndUpstreamKernelWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionThenVerifyzesDeviceGetPropertiesCallSucceedsAndDriverVersionIsReturned) {

    MockGlobalOperationsFsAccess *pFsAccess = new MockGlobalOperationsFsAccess();
    MockGlobalOperationsSysfsAccess *pSysfsAccess = new MockGlobalOperationsSysfsAccess();
    MockSysmanKmdInterfaceUpstream *pSysmanKmdInterface = new MockSysmanKmdInterfaceUpstream(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    pFsAccess->mockReadVal = srcVersion;
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == srcVersion.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileReadFailsThenVerifyzesDeviceGetPropertiesCallSucceeds) {

    MockGlobalOperationsFsAccess *pFsAccess = new MockGlobalOperationsFsAccess();
    MockGlobalOperationsSysfsAccess *pSysfsAccess = new MockGlobalOperationsSysfsAccess();
    MockSysmanKmdInterfaceUpstream *pSysmanKmdInterface = new MockSysmanKmdInterfaceUpstream(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    pFsAccess->mockReadError = ZE_RESULT_ERROR_UNKNOWN;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhenCallingZesDeviceGetPropertiesForCheckingDevicePropertiesWhenVendorIsUnKnownThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    pSysfsAccess->mockReadVal[static_cast<int>(MockGlobalOperationsSysfsAccess::Index::mockSubsystemVendor)] = "0xa086";
    pSysfsAccess->mockReadVal[static_cast<int>(MockGlobalOperationsSysfsAccess::Index::mockVendor)] = "0x1806"; // Unknown Vendor id
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.vendorName));
    EXPECT_TRUE(0 == unknown.compare(properties.brandName));
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

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenSuccessIsReturnedEvenwithFaultyClient) {
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

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenFailureIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    pSysfsAccess->mockGetScannedDirPidEntriesStatus = true;
    pSysfsAccess->mockReadError = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingBusyDirForEnginesReadThenFailureIsReturnedEvenwithFaultyClient) {
    uint32_t count = 0;
    pSysfsAccess->mockGetScannedDirPidEntriesStatus = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingBusyDirForEnginesThenFailureIsReturnedEvenwithFaultyClient) {
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
    auto pDrm = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->ioctlRetVal = -1;
    pDrm->ioctlErrno = EIO;
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrm));

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(ZES_RESET_REASON_FLAG_WEDGED, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceIsNotWedgedWhenCallingGetDeviceStateThenZeroIsReturned) {
    auto pDrm = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrm));

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenGemCreateIoctlFailsWithEINVALWhenCallingGetDeviceStateThenVerifyResetIsNotNeeded) {
    auto pDrm = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->ioctlRetVal = -1;
    pDrm->ioctlErrno = EINVAL;
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrm));

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingZesDeviceResetExtThenResetExtCallReturnSuccess) {
    init(true);

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_WARM};
    ze_result_t result = zesDeviceResetExt(pSysmanDevice->toHandle(), &pProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(pFsAccess->mockWarmResetValue, "1");

    pProperties.resetType = ZES_RESET_TYPE_COLD;
    result = zesDeviceResetExt(pSysmanDevice->toHandle(), &pProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(pFsAccess->mockColdResetValue, "1");

    pProperties.resetType = ZES_RESET_TYPE_FLR;
    result = zesDeviceResetExt(pSysmanDevice->toHandle(), &pProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(pFsAccess->mockFlrValue, "1");
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingZesDeviceResetExtForColdResetThenErrorIsReturned) {
    initGlobalOps();
    pProcfsAccess->ourDevicePid = pProcfsAccess->pidList[0];
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate
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

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingZesDeviceResetExtForWarmResetThenErrorIsReturned) {
    initGlobalOps();
    pProcfsAccess->ourDevicePid = pProcfsAccess->pidList[0];
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcfsAccess->isRepeated.push_back(true);
    pProcfsAccess->mockNoKill = true;
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_WARM};
    ze_result_t result = zesDeviceResetExt(device, &pProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingResetExtWithInvalidTypeThenFailureIsReturned) {
    init(true);
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_FORCE_UINT32};
    ze_result_t result = zesDeviceResetExt(pSysmanDevice->toHandle(), &pProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenGettingSysfsPathFailsWhenCallingResetExtThenFailureIsReturned) {
    init(true);
    pSysfsAccess->mockDeviceUnbound = true;
    zes_reset_properties_t pProperties = {.stype = ZES_STRUCTURE_TYPE_RESET_PROPERTIES, .pNext = nullptr, .force = true, .resetType = ZES_RESET_TYPE_FORCE_UINT32};
    ze_result_t result = zesDeviceResetExt(pSysmanDevice->toHandle(), &pProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueWhenCallingResetThenSuccessIsReturned) {
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenPermissionDeniedWhenCallingGetDeviceStateThenZeResultErrorInsufficientPermissionsIsReturned) {

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

TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenPermissionDeniedWhenCallingGetDeviceStateThenZeResultErrorInsufficientPermissionsIsReturned) {

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

    pProcfsAccess->ourDevicePid = pProcfsAccess->pidList[0];
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate

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
    MockGlobalOperationsProcfsAccess *pProcFsAccess = new MockGlobalOperationsProcfsAccess();

    pProcFsAccess->ourDevicePid = pProcFsAccess->pidList[0];
    pProcFsAccess->ourDeviceFd = pProcFsAccess->extraFd;
    pProcFsAccess->mockListProcessCall.push_back(DEVICE_UNUSED);
    pProcFsAccess->isRepeated.push_back(false);
    pProcFsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    pProcFsAccess->isRepeated.push_back(true);
    pProcFsAccess->mockNoKill = true;

    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    pSysmanKmdInterface->pProcfsAccess.reset(pProcFsAccess);

    std::unique_ptr<MockGlobalOpsLinuxSysmanImp> pMockGlobalOpsLinuxSysmanImp = std::make_unique<MockGlobalOpsLinuxSysmanImp>(pLinuxSysmanImp->getSysmanDeviceImp());
    pMockGlobalOpsLinuxSysmanImp->pProcfsAccess = pProcFsAccess;
    pMockGlobalOpsLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pLinuxSysmanImp = pMockGlobalOpsLinuxSysmanImp.get();
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->pProcfsAccess = pProcFsAccess;
    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate

    pMockGlobalOpsLinuxSysmanImp->ourDevicePid = pProcFsAccess->ourDevicePid;
    pMockGlobalOpsLinuxSysmanImp->ourDeviceFd = pProcFsAccess->extraFd;
    pMockGlobalOpsLinuxSysmanImp->setMockInitDeviceError(ZE_RESULT_ERROR_UNKNOWN);

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

TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenDeviceInUseWhenCallingResetAndListProcessesFailsThenZeResultErrorIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    pProcfsAccess->mockListProcessCall.push_back(RETURN_ERROR);
    pProcfsAccess->isRepeated.push_back(false);
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenProcessStartsMidResetWhenListProcessesFailsAfterUnbindThenFailureIsReturned) {

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

TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenProcessStartsMidResetWhenCallingResetAndWriteFailsAfterUnbindThenFailureIsReturned) {

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

TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenProcessStartsMidResetWhenCallingResetAndUnbindFailsThenFailureIsReturned) {

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

TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenProcessStartsMidResetWhenCallingResetAndGetFileNameFailsThenSuccessIsReturned) {

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
TEST_F(SysmanGlobalOperationsIntegratedFixture,
       GivenProcessWontDieWhenCallingResetThenZeResultErrorHandleObjectInUseErrorIsReturned) {
    initGlobalOps();
    //  Pretend another process has the device open
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

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhenCallingGetPropertiesThenDeviceTypeIsGpuInCoreProperties) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_DEVICE_TYPE_GPU, properties.core.type);
}

TEST_F(SysmanGlobalOperationsFixture,
       GivenValidDeviceHandleWhenCallingGetPropertiesAndOnDemandPageFaultNotSupportedThenFlagIsNotSetInCoreProperties) {
    auto mockHardwareInfo = device->getHardwareInfo();
    mockHardwareInfo.capabilityTable.supportsOnDemandPageFaults = false;
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(properties.core.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

HWTEST2_F(SysmanGlobalOperationsFixture,
          GivenValidDeviceHandleWhenCallingGetPropertiesnAndIsNotIntegratedDeviceThenFlagIsNotSetInCoreProperties, IsXeHpgCore) {
    auto mockHardwareInfo = device->getHardwareInfo();
    mockHardwareInfo.capabilityTable.isIntegratedDevice = false;
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(properties.core.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingGetPropertiesThenCorrectCorePropertiesAreReturned) {
    auto pHwInfo = pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->platform.usDeviceID = 0x1234;
    const std::string deviceName = "IntelTestDevice";
    pHwInfo->capabilityTable.deviceName = &deviceName[0];

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.type, ZE_DEVICE_TYPE_GPU);
    EXPECT_EQ(properties.core.vendorId, static_cast<uint32_t>(0x8086));
    EXPECT_EQ(properties.core.deviceId, static_cast<uint32_t>(pHwInfo->platform.usDeviceID));
    EXPECT_EQ(properties.core.coreClockRate, static_cast<uint32_t>(pHwInfo->capabilityTable.maxRenderFrequency));
    EXPECT_EQ(properties.core.maxHardwareContexts, static_cast<uint32_t>(1024 * 64));
    EXPECT_EQ(properties.core.maxCommandQueuePriority, 0u);
    EXPECT_EQ(properties.core.numThreadsPerEU, static_cast<uint32_t>(pHwInfo->gtSystemInfo.ThreadCount / pHwInfo->gtSystemInfo.EUCount));
    EXPECT_EQ(properties.core.numEUsPerSubslice, static_cast<uint32_t>(pHwInfo->gtSystemInfo.MaxEuPerSubSlice));
    EXPECT_EQ(properties.core.numSlices, static_cast<uint32_t>(pHwInfo->gtSystemInfo.SliceCount));
    EXPECT_EQ(properties.core.timestampValidBits, static_cast<uint32_t>(pHwInfo->capabilityTable.timestampValidBits));
    EXPECT_EQ(properties.core.kernelTimestampValidBits, static_cast<uint32_t>(pHwInfo->capabilityTable.kernelTimestampValidBits));
    EXPECT_TRUE(0 == deviceName.compare(properties.core.name));

    // Check if default device name is proper when HwInfo deviceName is null
    std::stringstream expectedDeviceName;
    expectedDeviceName << "Intel(R) Graphics";
    expectedDeviceName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << pSysmanDevice->getHardwareInfo().platform.usDeviceID << "]";

    pHwInfo->capabilityTable.deviceName = "";
    result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == expectedDeviceName.str().compare(properties.core.name));
}

TEST_F(SysmanGlobalOperationsFixture, WhenGettingDevicePropertiesThenSubslicesPerSliceIsBasedOnSubslicesSupported) {
    auto pHwInfo = pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->gtSystemInfo.MaxSubSlicesSupported = 48;
    pHwInfo->gtSystemInfo.MaxSlicesSupported = 3;
    pHwInfo->gtSystemInfo.SubSliceCount = 8;
    pHwInfo->gtSystemInfo.SliceCount = 1;

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(8u, properties.core.numSubslicesPerSlice);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingGetPropertiesThenCorrectTimerResolutionInCorePropertiesAreReturned) {
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime.reset(new NEO::MockOSTimeWithConstTimestamp());
    double mockedTimerResolution = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().osTime->getDynamicDeviceTimerResolution();
    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.timerResolution, static_cast<uint64_t>(mockedTimerResolution));

    properties = {};
    ze_device_properties_t coreProperties = {};
    coreProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
    properties.core = coreProperties;
    result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.timerResolution, static_cast<uint64_t>(1000000000.0 / mockedTimerResolution));

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseCyclesPerSecondTimer.set(1);
    properties = {};
    result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.timerResolution, static_cast<uint64_t>(1000000000.0 / mockedTimerResolution));
}

TEST_F(SysmanGlobalOperationsFixture, GivenDebugApiUsedSetWhenGettingDevicePropertiesThenSubslicesPerSliceIsBasedOnMaxSubslicesSupported) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebugApiUsed.set(1);
    auto pHwInfo = pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->gtSystemInfo.MaxSubSlicesSupported = 48;
    pHwInfo->gtSystemInfo.MaxSlicesSupported = 3;
    pHwInfo->gtSystemInfo.SubSliceCount = 8;
    pHwInfo->gtSystemInfo.SliceCount = 1;

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(16u, properties.core.numSubslicesPerSlice);
}

using SysmanDevicePropertiesExtensionTest = SysmanGlobalOperationsFixture;

TEST_F(SysmanDevicePropertiesExtensionTest,
       GivenValidDeviceHandleWhenCallingGetPropertiesForExtensionThenDeviceTypeIsGpu) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    zes_device_ext_properties_t extProperties = {ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES};
    properties.pNext = &extProperties;

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZES_DEVICE_TYPE_GPU, extProperties.type);
}

TEST_F(SysmanDevicePropertiesExtensionTest,
       GivenValidDeviceHandleWhenCallingGetPropertiesForExtensionAndOnDemandPageFaultNotSupportedThenFlagIsNotSet) {
    auto mockHardwareInfo = device->getHardwareInfo();
    mockHardwareInfo.capabilityTable.supportsOnDemandPageFaults = false;
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    zes_device_ext_properties_t extProperties = {ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES};
    properties.pNext = &extProperties;

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(extProperties.flags & ZES_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

HWTEST2_F(SysmanDevicePropertiesExtensionTest,
          GivenValidDeviceHandleWhenCallingGetPropertiesForExtensionAndIsNotIntegratedDeviceThenFlagIsNotSet, IsXeHpgCore) {
    auto mockHardwareInfo = device->getHardwareInfo();
    mockHardwareInfo.capabilityTable.isIntegratedDevice = false;
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    zes_device_ext_properties_t extProperties = {ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES};
    properties.pNext = &extProperties;

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(extProperties.flags & ZES_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

TEST_F(SysmanDevicePropertiesExtensionTest,
       GivenValidDeviceHandleWhenCallingGetPropertiesWithIncorrectStypeThenExtensionPropertiesNotSet) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    zes_device_ext_properties_t extProperties = {ZES_STRUCTURE_TYPE_ENGINE_EXT_PROPERTIES};
    properties.pNext = &extProperties;

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, extProperties.flags);
}

class SysmanDevicePropertiesExtensionTestMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->osTime = MockOSTime::create();
            execEnv->rootDeviceEnvironments[i]->osTime->setDeviceTimerResolution();
        }
    }
};

HWTEST2_F(SysmanDevicePropertiesExtensionTestMultiDevice,
          GivenValidDeviceHandleWhenCallingGetPropertiesForExtensionThenSubDeviceFlagSetCorrectly, IsXeHpcCore) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    zes_device_ext_properties_t extProperties = {ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES};
    properties.pNext = &extProperties;

    ze_result_t result = zesDeviceGetProperties(pSysmanDevice, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(extProperties.flags & ZES_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenCallingDeviceGetStateThenSuccessResultIsReturned) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrm));

    zes_device_state_t deviceState;
    ze_result_t result = zesDeviceGetState(pSysmanDevice, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

class SysmanGlobalOperationsUuidFixture : public SysmanDeviceFixture {

  public:
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;
    L0::Sysman::SysmanDeviceImp *device = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pGlobalOperationsImp = static_cast<L0::Sysman::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void setPciBusInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, const uint32_t rootDeviceIndex) {
        auto driverModel = std::make_unique<NEO::MockDriverModel>();
        driverModel->pciSpeedInfo = {1, 1, 1};
        driverModel->pciBusInfo = pciBusInfo;
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::move(driverModel));
    }

    void initGlobalOps() {
        zes_device_state_t deviceState;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    }
};

struct MockGlobalOperationsProductHelper : public ProductHelperHw<IGFX_UNKNOWN> {
    MockGlobalOperationsProductHelper() = default;
    bool getUuid(DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override {
        auto pDrm = driverModel->as<Drm>();
        if (pDrm->getFileDescriptor() >= 0) {
            return true;
        }
        return false;
    }
};

TEST_F(SysmanGlobalOperationsUuidFixture, GivenValidDeviceFDWhenRetrievingUuidThenValidFDIsVerifiedInProductHelper) {
    initGlobalOps();

    std::unique_ptr<ProductHelper> mockProductHelper = std::make_unique<MockGlobalOperationsProductHelper>();

    auto &rootDeviceEnvironment = device->getRootDeviceEnvironmentRef();

    std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    bool result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(true, result);

    std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);
}

using SysmanSubDevicePropertiesExperimentalTestMultiDevice = SysmanMultiDeviceFixture;
HWTEST2_F(SysmanSubDevicePropertiesExperimentalTestMultiDevice,
          GivenValidDeviceHandleWhenCallingGetSubDevicePropertiesThenApiSucceeds, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceGetSubDevicePropertiesExp(pSysmanDevice, &count, nullptr);
    EXPECT_TRUE(count > 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<zes_subdevice_exp_properties_t> subDeviceProps(count);
    result = zesDeviceGetSubDevicePropertiesExp(pSysmanDevice, &count, subDeviceProps.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(SysmanSubDevicePropertiesExperimentalTestMultiDevice,
          GivenValidDeviceHandleWhenCallingGetSubDevicePropertiesTwiceThenApiResultsMatch, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceGetSubDevicePropertiesExp(pSysmanDevice, &count, nullptr);
    EXPECT_EQ(true, (count > 0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<zes_subdevice_exp_properties_t> subDeviceProps(count);
    result = zesDeviceGetSubDevicePropertiesExp(pSysmanDevice, &count, subDeviceProps.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::vector<zes_subdevice_exp_properties_t> subDeviceProps2(count);
    result = zesDeviceGetSubDevicePropertiesExp(pSysmanDevice, &count, subDeviceProps2.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(subDeviceProps[i].subdeviceId, subDeviceProps2[i].subdeviceId);
        EXPECT_EQ(0, memcmp(subDeviceProps[i].uuid.id, subDeviceProps2[i].uuid.id, ZES_MAX_UUID_SIZE));
    }
}

class SysmanDeviceGetByUuidExperimentalTestMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->osTime = MockOSTime::create();
            execEnv->rootDeviceEnvironments[i]->osTime->setDeviceTimerResolution();
        }
    }
};

HWTEST2_F(SysmanDeviceGetByUuidExperimentalTestMultiDevice,
          GivenValidDriverHandleWhenCallingGetDeviceByUuidWithInvalidUuidThenErrorResultIsReturned, IsXeHpcCore) {
    zes_uuid_t uuid = {};
    zes_device_handle_t phDevice;
    ze_bool_t onSubdevice;
    uint32_t subdeviceId;

    ze_result_t result = zesDriverGetDeviceByUuidExp(driverHandle.get(), uuid, &phDevice, &onSubdevice, &subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(SysmanDeviceGetByUuidExperimentalTestMultiDevice,
          GivenValidDriverHandleWhenCallingGetDeviceByUuidWithValidUuidThenCorrectDeviceIsReturned, IsXeHpcCore) {

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zes_uuid_t uuid = {};
    std::copy_n(std::begin(properties.core.uuid.id), ZE_MAX_DEVICE_UUID_SIZE, std::begin(uuid.id));
    zes_device_handle_t phDevice;
    ze_bool_t onSubdevice;
    uint32_t subdeviceId;

    result = zesDriverGetDeviceByUuidExp(driverHandle.get(), uuid, &phDevice, &onSubdevice, &subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(phDevice, pSysmanDevice);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenValidDeviceHandleWhenCallingGenerateUuidFromPciBusInfoThenValidUuidIsReturned) {
    initGlobalOps();

    auto pHwInfo = pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->platform.usDeviceID = 0x1234;
    pHwInfo->platform.usRevId = 0x1;

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    NEO::PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = 0x5678;
    pciBusInfo.pciBus = 0x9;
    pciBusInfo.pciDevice = 0xA;
    pciBusInfo.pciFunction = 0xB;

    bool result = pGlobalOperationsImp->pOsGlobalOperations->generateUuidFromPciBusInfo(pciBusInfo, uuid);
    EXPECT_EQ(true, result);
    uint8_t *pUuid = (uint8_t *)uuid.data();
    EXPECT_EQ(*((uint16_t *)pUuid), (uint16_t)0x8086);
    EXPECT_EQ(*((uint16_t *)(pUuid + 2)), (uint16_t)pHwInfo->platform.usDeviceID);
    EXPECT_EQ(*((uint16_t *)(pUuid + 4)), (uint16_t)pHwInfo->platform.usRevId);
    EXPECT_EQ(*((uint16_t *)(pUuid + 6)), (uint16_t)pciBusInfo.pciDomain);
    EXPECT_EQ((*(pUuid + 8)), (uint8_t)pciBusInfo.pciBus);
    EXPECT_EQ((*(pUuid + 9)), (uint8_t)pciBusInfo.pciDevice);
    EXPECT_EQ((*(pUuid + 10)), (uint8_t)pciBusInfo.pciFunction);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenValidDeviceHandleWithInvalidPciDomainWhenCallingGenerateUuidFromPciBusInfoThenFalseIsReturned) {
    initGlobalOps();

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    NEO::PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();

    bool result = pGlobalOperationsImp->pOsGlobalOperations->generateUuidFromPciBusInfo(pciBusInfo, uuid);
    EXPECT_EQ(false, result);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenNullOsInterfaceObjectWhenRetrievingUuidThenFalseIsReturned) {
    initGlobalOps();

    auto &rootDeviceEnvironment = (pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());
    auto prevOsInterface = std::move(rootDeviceEnvironment.osInterface);
    rootDeviceEnvironment.osInterface = nullptr;

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    bool result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(false, result);
    rootDeviceEnvironment.osInterface = std::move(prevOsInterface);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenInvalidPciBusInfoWhenRetrievingUuidThenFalseIsReturned) {
    initGlobalOps();
    auto prevFlag = debugManager.flags.EnableChipsetUniqueUUID.get();
    debugManager.flags.EnableChipsetUniqueUUID.set(0);

    PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();
    setPciBusInfo(execEnv, pciBusInfo, 0);
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    bool result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(false, result);
    debugManager.flags.EnableChipsetUniqueUUID.set(prevFlag);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

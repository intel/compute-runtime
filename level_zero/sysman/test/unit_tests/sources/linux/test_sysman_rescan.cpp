/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/driver/os_sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_rescan.h"
#include "level_zero/zes_intel_gpu_sysman.h"

#include <fcntl.h>

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanRescanLinuxDriverImpTest = SysmanDeviceFixture;

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDriverHandleWithoutExecutionEnvironmentWhenCallingRescanDevicesThenUninitializedIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    auto emptyDriverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
    ASSERT_EQ(nullptr, emptyDriverHandle->getExecutionEnvironment());

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pLinuxDriverImp->rescanDevices(emptyDriverHandle.get(), &count, nullptr));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenNonDrmHwDeviceIdWhenCallingGetPciBdfAndGetPciUuidForHwDeviceThenErrorUnknownIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    class NonDrmHwDeviceId : public NEO::HwDeviceId {
      public:
        NonDrmHwDeviceId() : NEO::HwDeviceId(NEO::DriverModelType::wddm) {}
    };
    NonDrmHwDeviceId hwDeviceId;

    std::string pciBdf;
    std::string pciUuid;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxDriverImp->getPciBdfForHwDevice(&hwDeviceId, pciBdf));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxDriverImp->getPciUuidForHwDevice(&hwDeviceId, pciUuid));
    EXPECT_TRUE(pciUuid.empty());
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndValidUuidFileWhenCallingGetPciBdfAndGetPciUuidForHwDeviceThenBdfAndUuidAreReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    const std::string mockPciPath = "0000:03:00.0";
    const std::string mockUuidValue = "01234567-89ab-cdef-0123-456789abcdef";

    NEO::HwDeviceIdDrm hwDeviceId(-1, mockPciPath.c_str());

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (std::string(pathname).find("device_uuid") != std::string::npos) {
            return 7;
        }
        return -1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        const std::string value = "01234567-89ab-cdef-0123-456789abcdef\n";
        auto size = std::min(count, value.size());
        memcpy(buf, value.data(), size);
        return static_cast<ssize_t>(size);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    std::string pciBdf;
    std::string pciUuid;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciBdfForHwDevice(&hwDeviceId, pciBdf));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciUuidForHwDevice(&hwDeviceId, pciUuid));
    EXPECT_EQ(mockPciPath, pciBdf);
    EXPECT_EQ(mockUuidValue, pciUuid);
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenUuidFileWithTrailingNewlineWhenReadingUuidForHwDeviceAndForSysmanDeviceThenBothUuidsAreEqual) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:03:00.0");

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 7;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        const std::string value = "01234567-89ab-cdef-0123-456789abcdef\n";
        auto size = std::min(count, value.size());
        memcpy(buf, value.data(), size);
        return static_cast<ssize_t>(size);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    std::string hwDeviceUuid;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciUuidForHwDevice(&hwDeviceId, hwDeviceUuid));

    pLinuxSysmanImp->pciUuid.clear();
    EXPECT_EQ(hwDeviceUuid, pLinuxSysmanImp->getPciUuid());
    EXPECT_EQ("01234567-89ab-cdef-0123-456789abcdef", hwDeviceUuid);
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndUuidFileOpenFailsWhenCallingGetPciUuidForHwDeviceThenErrorIsReturnedAndUuidIsEmpty) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:03:00.0");

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });

    std::string pciUuid;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxDriverImp->getPciUuidForHwDevice(&hwDeviceId, pciUuid));
    EXPECT_TRUE(pciUuid.empty());
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndUuidFileReadFailsWhenCallingGetPciUuidForHwDeviceThenErrorIsReturnedAndUuidIsEmpty) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:03:00.0");

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 7;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        return 0;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    std::string pciUuid;
    EXPECT_NE(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciUuidForHwDevice(&hwDeviceId, pciUuid));
    EXPECT_TRUE(pciUuid.empty());
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenNonDrmDriverModelWhenCallingUpdateHwDeviceIdThenUnsupportedFeatureIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    pSysmanDeviceImp->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>(NEO::DriverModelType::wddm));

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:ab:00.0", "/dev/dri/renderD128");
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, &hwDeviceId));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenNonDrmHwDeviceIdWhenCallingUpdateHwDeviceIdThenUnsupportedFeatureIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    class NonDrmHwDeviceId : public NEO::HwDeviceId {
      public:
        NonDrmHwDeviceId() : NEO::HwDeviceId(NEO::DriverModelType::wddm) {}
    };
    NonDrmHwDeviceId hwDeviceId;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, &hwDeviceId));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDiscoveredHwDeviceIdAtNewBdfWhenCallingUpdateHwDeviceIdThenSuccessIsReturnedAndDrmBdfIsUpdated) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:ab:00.0", "/dev/dri/renderD128");
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, &hwDeviceId));

    auto pciBusInfo = pLinuxSysmanImp->getDrm()->getPciBusInfo();
    EXPECT_EQ(0xabu, pciBusInfo.pciBus);
    EXPECT_EQ(0x0u, pciBusInfo.pciDevice);
    EXPECT_EQ(0x0u, pciBusInfo.pciFunction);
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenQueryAdapterBdfFailsWhenCallingUpdateHwDeviceIdThenUninitializedIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    auto pMockDrm = new MockRescanDrm(pSysmanDeviceImp->getRootDeviceEnvironmentRef());
    pMockDrm->mockQueryAdapterBdfResult = 1;
    pSysmanDeviceImp->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(pMockDrm));

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:ab:00.0", "/dev/dri/renderD128");
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, &hwDeviceId));
}

using SysmanRescanLinuxImpTest = SysmanDeviceFixture;

TEST_F(SysmanRescanLinuxImpTest, GivenProcfsGetFileNameSucceedsWhenCallingUpdateBdfDependentDataThenSuccessIsReturnedAndSysfsPathIsRefreshed) {
    auto pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
    auto pMockProcfsAccess = std::make_unique<MockRescanProcfsAccess>();
    auto pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
    auto pOrigProcfsAccess = pLinuxSysmanImp->pProcfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();
    pLinuxSysmanImp->pProcfsAccess = pMockProcfsAccess.get();

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->updateBdfDependentData());
    EXPECT_GE(pMockSysfsAccess->getRealPathCalled, 1u);

    pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pOrigProcfsAccess;
}

TEST_F(SysmanRescanLinuxImpTest, GivenProcfsGetFileNameFailsWhenCallingUpdateBdfDependentDataThenErrorIsReturnedAndSysfsPathIsNotRefreshed) {
    auto pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
    auto pMockProcfsAccess = std::make_unique<MockRescanProcfsAccess>();
    pMockProcfsAccess->mockGetFileNameResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
    auto pOrigProcfsAccess = pLinuxSysmanImp->pProcfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();
    pLinuxSysmanImp->pProcfsAccess = pMockProcfsAccess.get();

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->updateBdfDependentData());
    EXPECT_EQ(0u, pMockSysfsAccess->getRealPathCalled);

    pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pOrigProcfsAccess;
}

TEST_F(SysmanRescanLinuxImpTest, GivenCachedFdsWhenCallingUpdateBdfDependentDataThenFsAndSysfsFdCachesAreCleared) {
    auto pMockFsAccess = std::make_unique<MockRescanFsAccess>();
    auto pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
    auto pMockProcfsAccess = std::make_unique<MockRescanProcfsAccess>();
    auto pOrigFsAccess = pLinuxSysmanImp->pFsAccess;
    auto pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
    auto pOrigProcfsAccess = pLinuxSysmanImp->pProcfsAccess;
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();
    pLinuxSysmanImp->pProcfsAccess = pMockProcfsAccess.get();

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->updateBdfDependentData());
    EXPECT_EQ(1u, pMockFsAccess->clearFdCacheCalled);
    EXPECT_EQ(1u, pMockSysfsAccess->clearFdCacheCalled);

    pLinuxSysmanImp->pFsAccess = pOrigFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pOrigProcfsAccess;
}

TEST_F(SysmanRescanLinuxImpTest, GivenSysfsReadSucceedsWhenCallingGetPciUuidThenUuidIsReturnedAndCached) {
    auto pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
    auto pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

    std::string uuid = pLinuxSysmanImp->getPciUuid();
    EXPECT_EQ(pMockSysfsAccess->mockUuid, uuid);

    pMockSysfsAccess->mockReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(pMockSysfsAccess->mockUuid, pLinuxSysmanImp->getPciUuid());

    pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
}

TEST_F(SysmanRescanLinuxImpTest, GivenSysfsReadFailsWhenCallingGetPciUuidThenEmptyStringIsReturned) {
    auto pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
    pMockSysfsAccess->mockReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

    EXPECT_TRUE(pLinuxSysmanImp->getPciUuid().empty());

    pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
}

TEST_F(SysmanRescanLinuxImpTest, GivenNullPciHandleWhenCallingReInitSysmanDeviceCacheThenPciInitIsSkippedAndCallSucceeds) {
    auto pOrigPci = pSysmanDeviceImp->pPci;
    pSysmanDeviceImp->pPci = nullptr;

    pLinuxSysmanImp->reInitSysmanDeviceCache();

    pSysmanDeviceImp->pPci = pOrigPci;
}

class SysmanRescanDriverHandleTest : public SysmanDeviceFixture {
  public:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle.get());
        pMockOsSysmanDriver = std::make_unique<MockRescanOsSysmanDriver>();
        pOrigOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
        pSysmanDriverHandleImp->pOsSysmanDriver = pMockOsSysmanDriver.get();
        pciDevicesDirectoryBackup = std::make_unique<VariableBackup<const char *>>(&Os::pciDevicesDirectory, "/");
        directoryFilesMapBackup = std::make_unique<VariableBackup<std::map<std::string, std::vector<std::string>>>>(&NEO::directoryFilesMap);

        pLinuxSysmanImp->pciUuid = mockRescanPciUuid;
    }
    void TearDown() override {
        pSysmanDriverHandleImp->pOsSysmanDriver = pOrigOsSysmanDriver;
        SysmanDeviceFixture::TearDown();
    }

    void setupSingleDiscoveredDevice(const std::string &bdf) {
        NEO::directoryFilesMap.clear();
        NEO::directoryFilesMap[Os::pciDevicesDirectory] = {};
        NEO::directoryFilesMap[Os::pciDevicesDirectory].push_back(std::string("/pci-") + bdf + "-render");
    }

    L0::Sysman::SysmanDriverHandleImp *pSysmanDriverHandleImp = nullptr;
    std::unique_ptr<MockRescanOsSysmanDriver> pMockOsSysmanDriver;
    L0::Sysman::OsSysmanDriver *pOrigOsSysmanDriver = nullptr;
    std::unique_ptr<VariableBackup<const char *>> pciDevicesDirectoryBackup;
    std::unique_ptr<VariableBackup<std::map<std::string, std::vector<std::string>>>> directoryFilesMapBackup;
};

TEST_F(SysmanRescanDriverHandleTest, GivenNullOsSysmanDriverWhenCallingGetDeviceRescanThenUninitializedIsReturned) {
    auto savedOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));

    pSysmanDriverHandleImp->pOsSysmanDriver = savedOsSysmanDriver;
}

TEST_F(SysmanRescanDriverHandleTest, GivenCountZeroAndNonNullDevicesWhenCallingGetDeviceRescanThenDiscoveryLoopRunsAndDeviceCountIsReturned) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";

    uint32_t count = 0;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    EXPECT_EQ(0u, count);
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

TEST_F(SysmanRescanDriverHandleTest, GivenCountZeroAndNullDevicesWhenCallingGetDeviceRescanThenDiscoveredDeviceCountIsReturned) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanRescanDriverHandleTest, GivenGetPciBdfFailsWhenCallingGetDeviceRescanThenErrorIsPropagated) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    pMockOsSysmanDriver->mockGetPciBdfResult = ZE_RESULT_ERROR_UNKNOWN;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
}

TEST_F(SysmanRescanDriverHandleTest, GivenGetPciUuidFailsAndNoKnownDeviceAtBdfWhenCallingGetDeviceRescanThenDeviceIsSkipped) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ee:00.0");

    pMockOsSysmanDriver->mockPciBdf = "0000:ee:00.0";
    pMockOsSysmanDriver->mockGetPciUuidResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

TEST_F(SysmanRescanDriverHandleTest, GivenGetPciUuidFailsAndKnownDeviceAtBdfWhenCallingGetDeviceRescanThenDeviceAtBdfIsRefreshed) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo(0, 0xab, 0, 0);
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockGetPciUuidResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
    EXPECT_EQ(1u, pMockOsSysman->updateBdfDependentDataCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenGetPciUuidFailsAndUnparsableBdfWhenCallingGetDeviceRescanThenDependencyUnavailableIsReturnedAndDeviceIsNotRelocated) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    pMockOsSysmanDriver->mockPciBdf = "bad-bdf";
    pMockOsSysmanDriver->mockGetPciUuidResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));
    EXPECT_EQ(1u, count);

    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

TEST_F(SysmanRescanDriverHandleTest, GivenUnknownPciUuidWhenCallingGetDeviceRescanThenDeviceIsSkipped) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceWithEmptyPciUuidWhenCallingGetSysmanDeviceByPciUuidWithEmptyUuidThenNullptrIsReturned) {
    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = "";
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(pSysmanDriverHandleImp, ""));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenBdfChangedForCachedUuidWhenCallingGetDeviceRescanThenDeviceIsRelocatedAndBdfDependentDataUpdated) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
    EXPECT_EQ(1u, pMockOsSysman->updateBdfDependentDataCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenBdfChangedForCachedUuidWhenCallingGetDeviceRescanThenUuidDeviceMapKeepsUuidsRegisteredAtInit) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");
    const auto uuidDeviceMapAtInit = pSysmanDriverHandleImp->getUuidDeviceMap();
    ASSERT_FALSE(uuidDeviceMapAtInit.empty());

    const std::string relocatedUuid = "relocated-device-uuid";
    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockDeviceUuids = true;
    pMockOsSysman->mockDeviceUuids = {relocatedUuid};
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    EXPECT_EQ(uuidDeviceMapAtInit, pSysmanDriverHandleImp->getUuidDeviceMap());
    EXPECT_FALSE(pSysmanDriverHandleImp->getUuidDeviceMap().contains(relocatedUuid));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenUpdateHwDeviceIdFailsForRelocatedDeviceWhenCallingGetDeviceRescanThenErrorIsPropagated) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;
    pMockOsSysmanDriver->mockUpdateHwDeviceIdResult = ZE_RESULT_ERROR_DEVICE_LOST;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenUpdateBdfDependentDataFailsForRelocatedDeviceWhenCallingGetDeviceRescanThenErrorIsPropagated) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->mockUpdateBdfResult = ZE_RESULT_ERROR_UNINITIALIZED;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenBdfUnchangedForCachedUuidWhenCallingGetDeviceRescanThenDeviceIsRefreshedInPlace) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo(0, 0xab, 0, 0);
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(2u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
    EXPECT_EQ(2u, pMockOsSysman->updateBdfDependentDataCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenKnownDeviceNotDiscoveredWhenCallingGetDeviceRescanThenDeviceIsMovedToLostListAndPciLookupsNoLongerFindIt) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";
    const NEO::PhysicalDevicePciBusInfo lastSeenBusInfo(0, 0x03, 0, 0);
    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = lastSeenBusInfo;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();
    ASSERT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, lastSeenBusInfo));
    const auto uuidDeviceMapAtInit = pSysmanDriverHandleImp->getUuidDeviceMap();
    ASSERT_FALSE(uuidDeviceMapAtInit.empty());

    uint32_t count = 1;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    EXPECT_EQ(0u, count);
    EXPECT_EQ(nullptr, handle);

    EXPECT_TRUE(pSysmanDriverHandleImp->sysmanDevices.empty());
    EXPECT_EQ(0u, pSysmanDriverHandleImp->numDevices);
    ASSERT_EQ(1u, pSysmanDriverHandleImp->lostSysmanDevices.size());
    EXPECT_EQ(pSysmanDevice, pSysmanDriverHandleImp->lostSysmanDevices[0]);
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(pSysmanDriverHandleImp, mockRescanPciUuid));
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, lastSeenBusInfo));
    EXPECT_EQ(uuidDeviceMapAtInit, pSysmanDriverHandleImp->getUuidDeviceMap());

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenLostDeviceHandleWhenCallingSysmanApiThenNullDeviceIsResolvedAndDeviceLostIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";

    uint32_t count = 1;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    ASSERT_TRUE(pSysmanDriverHandleImp->isLostSysmanDevice(pSysmanDevice));

    zes_device_handle_t staleHandle = pSysmanDevice->toHandle();
    EXPECT_EQ(nullptr, L0::Sysman::SysmanDevice::fromHandle(staleHandle));
    zes_device_state_t state = {ZES_STRUCTURE_TYPE_DEVICE_STATE};
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, L0::Sysman::SysmanDevice::deviceGetState(staleHandle, &state));
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, L0::Sysman::SysmanDevice::getUnresolvedHandleResult(staleHandle));
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceNotLostWhenCallingGetUnresolvedHandleResultThenUninitializedIsReturned) {
    ASSERT_FALSE(pSysmanDriverHandleImp->isLostSysmanDevice(pSysmanDevice));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, L0::Sysman::SysmanDevice::getUnresolvedHandleResult(pSysmanDevice->toHandle()));

    VariableBackup<L0::Sysman::SysmanDriverHandleImp *> globalDriverBackup(&L0::Sysman::globalSysmanDriver, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, L0::Sysman::SysmanDevice::getUnresolvedHandleResult(pSysmanDevice->toHandle()));
}

TEST_F(SysmanRescanDriverHandleTest, GivenLostDeviceHandleWhenListeningForEventsThenDeviceLostIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";

    uint32_t count = 1;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    ASSERT_TRUE(pSysmanDriverHandleImp->isLostSysmanDevice(pSysmanDevice));

    zes_device_handle_t staleHandle = pSysmanDevice->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t deviceEvents = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, pMockOsSysmanDriver->eventsListen(0u, 1u, &staleHandle, &numDeviceEvents, &deviceEvents));
}

TEST_F(SysmanRescanDriverHandleTest, GivenAllDevicesLostWhenCallingGetDeviceRescanAgainThenDiscoveredDevicesAreStillCounted) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";

    uint32_t count = 1;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    ASSERT_TRUE(pSysmanDriverHandleImp->sysmanDevices.empty());

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceRelocatedWhenCallingGetDeviceRescanThenBdfLookupFindsDeviceAtNewBdfOnly) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    const NEO::PhysicalDevicePciBusInfo oldBusInfo(0, 0x03, 0, 0);
    const NEO::PhysicalDevicePciBusInfo newBusInfo(0, 0xab, 0, 0);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = oldBusInfo;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    EXPECT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, oldBusInfo));

    pMockOsSysman->mockPciBdfInfo = newBusInfo;
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, oldBusInfo));
    EXPECT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, newBusInfo));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceReportsInvalidPciBusInfoWhenCallingGetSysmanDeviceByPciBusInfoThenDeviceIsNotMatched) {
    const NEO::PhysicalDevicePciBusInfo busInfo(0, 0x03, 0, 0);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = busInfo;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();
    EXPECT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo::invalid();
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceReportsNullPciBusInfoWhenCallingGetSysmanDeviceByPciBusInfoThenDeviceIsNotMatched) {
    const NEO::PhysicalDevicePciBusInfo busInfo(0, 0x03, 0, 0);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = busInfo;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();
    EXPECT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    pMockOsSysman->returnNullPciBdfInfo = true;
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenUnknownPciBusInfoWhenCallingGetSysmanDeviceByPciBusInfoThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, NEO::PhysicalDevicePciBusInfo(0xffff, 0xff, 0x1f, 0x7)));
}

TEST_F(SysmanRescanDriverHandleTest, GivenPciBusInfoDifferingOnlyInDeviceOrFunctionWhenCallingGetSysmanDeviceByPciBusInfoThenDeviceIsNotMatched) {
    const NEO::PhysicalDevicePciBusInfo busInfo(0, 0x03, 0x1, 0x1);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = busInfo;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();
    EXPECT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, NEO::PhysicalDevicePciBusInfo(0, 0x03, 0x2, 0x1)));
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, NEO::PhysicalDevicePciBusInfo(0, 0x03, 0x1, 0x2)));

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceInSurvivabilityModeWhenLookingUpByPciUuidOrPciBusInfoThenDeviceIsNotMatched) {
    const NEO::PhysicalDevicePciBusInfo busInfo(0, 0x03, 0, 0);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = busInfo;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();
    ASSERT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(pSysmanDriverHandleImp, mockRescanPciUuid));
    ASSERT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    pSysmanDevice->isDeviceInSurvivabilityMode = true;
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(pSysmanDriverHandleImp, mockRescanPciUuid));
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, busInfo));

    pSysmanDevice->isDeviceInSurvivabilityMode = false;
    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenDeviceInSurvivabilityModeNotFoundWhenCallingRemoveLostSysmanDevicesThenDeviceIsKept) {
    pSysmanDevice->isDeviceInSurvivabilityMode = true;

    LinuxSysmanDriverImp::removeLostSysmanDevices(pSysmanDriverHandleImp, {});

    ASSERT_EQ(1u, pSysmanDriverHandleImp->sysmanDevices.size());
    EXPECT_EQ(pSysmanDevice, pSysmanDriverHandleImp->sysmanDevices[0]);
    EXPECT_TRUE(pSysmanDriverHandleImp->lostSysmanDevices.empty());

    pSysmanDevice->isDeviceInSurvivabilityMode = false;
}

TEST_F(SysmanRescanDriverHandleTest, GivenUnknownPciUuidWhenCallingGetSysmanDeviceByPciUuidThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(pSysmanDriverHandleImp, "uuid-not-in-map"));
    EXPECT_EQ(pSysmanDevice, LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(pSysmanDriverHandleImp, mockRescanPciUuid));
}

TEST_F(SysmanRescanDriverHandleTest, GivenKnownUuidWhenCallingGetDeviceRescanThenDiscoveredHwDeviceIdIsPassedToUpdateHwDeviceId) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
    EXPECT_EQ("0000:ab:00.0", pMockOsSysmanDriver->lastUpdateHwDeviceIdPciPath);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

class SysmanRescanPciUuidReadAtInitTest : public SysmanDeviceFixture {
  public:
    static constexpr int pciUuidFileDescriptor = 0x7a;
    static inline const std::string pciUuidAtInit = "fedcba98-7654-3210-fedc-ba9876543210";

    void SetUp() override {
        VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
            if (std::string_view(pathname).find("device_uuid") != std::string_view::npos) {
                return pciUuidFileDescriptor;
            }
            return NEO::SysCalls::openFuncRetVal;
        });
        VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
            if (fd != pciUuidFileDescriptor) {
                return 0;
            }
            const std::string value = pciUuidAtInit + "\n";
            auto size = std::min(count, value.size());
            memcpy(buf, value.data(), size);
            return static_cast<ssize_t>(size);
        });
        SysmanDeviceFixture::SetUp();

        pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle.get());
        pMockOsSysmanDriver = std::make_unique<MockRescanOsSysmanDriver>();
        pOrigOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
        pSysmanDriverHandleImp->pOsSysmanDriver = pMockOsSysmanDriver.get();

        pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
        pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pMockProcfsAccess = std::make_unique<MockRescanProcfsAccess>();
        pOrigProcfsAccess = pLinuxSysmanImp->pProcfsAccess;

        pciDevicesDirectoryBackup = std::make_unique<VariableBackup<const char *>>(&Os::pciDevicesDirectory, "/");
        directoryFilesMapBackup = std::make_unique<VariableBackup<std::map<std::string, std::vector<std::string>>>>(&NEO::directoryFilesMap);
    }
    void TearDown() override {
        pLinuxSysmanImp->pProcfsAccess = pOrigProcfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
        pSysmanDriverHandleImp->pOsSysmanDriver = pOrigOsSysmanDriver;
        SysmanDeviceFixture::TearDown();
    }

    L0::Sysman::SysmanDriverHandleImp *pSysmanDriverHandleImp = nullptr;
    std::unique_ptr<MockRescanOsSysmanDriver> pMockOsSysmanDriver;
    std::unique_ptr<MockRescanSysfsAccess> pMockSysfsAccess;
    std::unique_ptr<MockRescanProcfsAccess> pMockProcfsAccess;
    L0::Sysman::OsSysmanDriver *pOrigOsSysmanDriver = nullptr;
    L0::Sysman::SysFsAccessInterface *pOrigSysfsAccess = nullptr;
    L0::Sysman::ProcFsAccessInterface *pOrigProcfsAccess = nullptr;
    std::unique_ptr<VariableBackup<const char *>> pciDevicesDirectoryBackup;
    std::unique_ptr<VariableBackup<std::map<std::string, std::vector<std::string>>>> directoryFilesMapBackup;
};

TEST_F(SysmanRescanPciUuidReadAtInitTest, GivenCardNodeRenumberedToAnotherGpuBeforeFirstRescanWhenCallingGetDeviceRescanThenDeviceIsMatchedByPciUuidReadAtInit) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    pMockSysfsAccess->mockUuid = "00000000-1111-2222-3333-444444444444";
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();
    pLinuxSysmanImp->pProcfsAccess = pMockProcfsAccess.get();

    NEO::directoryFilesMap.clear();
    NEO::directoryFilesMap[Os::pciDevicesDirectory] = {"/pci-0000:ab:00.0-render"};
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = pciUuidAtInit;

    uint32_t count = 1;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(pSysmanDevice->toHandle(), handle);
    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
    EXPECT_TRUE(pSysmanDriverHandleImp->lostSysmanDevices.empty());
}

class SysmanRescanTwoDevicesTest : public SysmanRescanDriverHandleTest {
  public:
    void SetUp() override {
        numRootDevices = 2u;
        SysmanRescanDriverHandleTest::SetUp();
    }
};

TEST_F(SysmanRescanTwoDevicesTest, GivenFirstDeviceLostWhenCallingGetDeviceRescanTwiceThenCountsMatchAndOnlySurvivingDeviceIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");
    ASSERT_EQ(2u, pSysmanDriverHandleImp->sysmanDevices.size());
    auto pLostDevice = pSysmanDriverHandleImp->sysmanDevices[0];
    auto pSurvivingDevice = pSysmanDriverHandleImp->sysmanDevices[1];
    auto pSurvivingDeviceImp = static_cast<SysmanDeviceImp *>(pSurvivingDevice);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSurvivingDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    auto pOrigOsSysman = pSurvivingDeviceImp->pOsSysman;
    pSurvivingDeviceImp->pOsSysman = pMockOsSysman.get();
    pLinuxSysmanImp->pciUuid = "lost-device-uuid";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));
    EXPECT_EQ(1u, count);

    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, count);
    EXPECT_EQ(pSurvivingDevice->toHandle(), handles[0]);
    ASSERT_EQ(1u, pSysmanDriverHandleImp->sysmanDevices.size());
    EXPECT_EQ(pSurvivingDevice, pSysmanDriverHandleImp->sysmanDevices[0]);
    ASSERT_EQ(1u, pSysmanDriverHandleImp->lostSysmanDevices.size());
    EXPECT_EQ(pLostDevice, pSysmanDriverHandleImp->lostSysmanDevices[0]);
    EXPECT_EQ(1u, pMockOsSysman->updateBdfDependentDataCalled);

    pSurvivingDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanTwoDevicesTest, GivenDeviceInSurvivabilityModeWhenCallingGetDeviceRescanTwiceThenCountIncludesItAndItIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");
    ASSERT_EQ(2u, pSysmanDriverHandleImp->sysmanDevices.size());
    auto pDiscoveredDevice = pSysmanDriverHandleImp->sysmanDevices[0];
    auto pSurvivabilityDevice = pSysmanDriverHandleImp->sysmanDevices[1];
    auto pDiscoveredDeviceImp = static_cast<SysmanDeviceImp *>(pDiscoveredDevice);

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pDiscoveredDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    auto pOrigOsSysman = pDiscoveredDeviceImp->pOsSysman;
    pDiscoveredDeviceImp->pOsSysman = pMockOsSysman.get();
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;
    pSurvivabilityDevice->isDeviceInSurvivabilityMode = true;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));
    EXPECT_EQ(2u, count);

    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(2u, count);
    EXPECT_EQ(pDiscoveredDevice->toHandle(), handles[0]);
    EXPECT_EQ(pSurvivabilityDevice->toHandle(), handles[1]);
    EXPECT_TRUE(pSysmanDriverHandleImp->lostSysmanDevices.empty());
    EXPECT_EQ(1u, pMockOsSysman->updateBdfDependentDataCalled);

    pSurvivabilityDevice->isDeviceInSurvivabilityMode = false;
    pDiscoveredDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanTwoDevicesTest, GivenAllKnownDevicesFoundWhenCallingRemoveLostSysmanDevicesThenDeviceListAndOrderAreUnchanged) {
    auto devicesBefore = pSysmanDriverHandleImp->sysmanDevices;

    LinuxSysmanDriverImp::removeLostSysmanDevices(pSysmanDriverHandleImp, {devicesBefore[1], devicesBefore[0]});

    EXPECT_EQ(devicesBefore, pSysmanDriverHandleImp->sysmanDevices);
    EXPECT_EQ(2u, pSysmanDriverHandleImp->numDevices);
    EXPECT_TRUE(pSysmanDriverHandleImp->lostSysmanDevices.empty());
}

TEST_F(SysmanRescanTwoDevicesTest, GivenBdfOfSecondDeviceWhenCallingGetSysmanDeviceByPciBusInfoThenSecondDeviceIsReturned) {
    const NEO::PhysicalDevicePciBusInfo firstBusInfo(0, 0x03, 0, 0);
    const NEO::PhysicalDevicePciBusInfo secondBusInfo(0, 0x04, 0, 0);
    std::vector<std::unique_ptr<MockRescanOsSysman>> mockOsSysmans;
    std::vector<OsSysman *> origOsSysmans;
    for (auto sysmanDevice : pSysmanDriverHandleImp->sysmanDevices) {
        auto sysmanDeviceImp = static_cast<SysmanDeviceImp *>(sysmanDevice);
        auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(sysmanDeviceImp);
        pMockOsSysman->useMockPciBdfInfo = true;
        pMockOsSysman->mockPciBdfInfo = mockOsSysmans.empty() ? firstBusInfo : secondBusInfo;
        origOsSysmans.push_back(sysmanDeviceImp->pOsSysman);
        sysmanDeviceImp->pOsSysman = pMockOsSysman.get();
        mockOsSysmans.push_back(std::move(pMockOsSysman));
    }

    EXPECT_EQ(pSysmanDriverHandleImp->sysmanDevices[0], LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, firstBusInfo));
    EXPECT_EQ(pSysmanDriverHandleImp->sysmanDevices[1], LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(pSysmanDriverHandleImp, secondBusInfo));

    for (uint32_t i = 0; i < origOsSysmans.size(); i++) {
        static_cast<SysmanDeviceImp *>(pSysmanDriverHandleImp->sysmanDevices[i])->pOsSysman = origOsSysmans[i];
    }
}

TEST_F(SysmanRescanTwoDevicesTest, GivenTwoDevicesSwappedBdfsWhenReinitializingTheirCachedInfoThenEachInitUuidStillMapsToItsOwnDevice) {
    const std::vector<std::string> initUuids = {"first-device-uuid", "second-device-uuid"};
    std::vector<SysmanDeviceImp *> devices;
    std::vector<std::unique_ptr<MockRescanOsSysman>> mockOsSysmans;
    std::vector<OsSysman *> origOsSysmans;
    for (uint32_t i = 0; i < initUuids.size(); i++) {
        auto sysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDriverHandleImp->sysmanDevices[i]);
        auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(sysmanDeviceImp);
        pMockOsSysman->useMockDeviceUuids = true;
        pMockOsSysman->mockDeviceUuids = {initUuids[i]};
        origOsSysmans.push_back(sysmanDeviceImp->pOsSysman);
        sysmanDeviceImp->pOsSysman = pMockOsSysman.get();
        pSysmanDriverHandleImp->updateUuidMap(sysmanDeviceImp);
        devices.push_back(sysmanDeviceImp);
        mockOsSysmans.push_back(std::move(pMockOsSysman));
    }

    mockOsSysmans[0]->mockDeviceUuids = {initUuids[1]};
    mockOsSysmans[1]->mockDeviceUuids = {initUuids[0]};
    NEO::HwDeviceIdDrm firstHwDeviceId(-1, "0000:04:00.0", "/dev/dri/renderD129");
    NEO::HwDeviceIdDrm secondHwDeviceId(-1, "0000:03:00.0", "/dev/dri/renderD128");
    EXPECT_EQ(ZE_RESULT_SUCCESS, pMockOsSysmanDriver->reinitDeviceCachedInfo(devices[0], &firstHwDeviceId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pMockOsSysmanDriver->reinitDeviceCachedInfo(devices[1], &secondHwDeviceId));

    const auto &uuidDeviceMap = pSysmanDriverHandleImp->getUuidDeviceMap();
    for (uint32_t i = 0; i < initUuids.size(); i++) {
        auto it = uuidDeviceMap.find(initUuids[i]);
        ASSERT_NE(it, uuidDeviceMap.end());
        EXPECT_EQ(static_cast<SysmanDevice *>(devices[i]), it->second);
    }

    for (uint32_t i = 0; i < devices.size(); i++) {
        devices[i]->pOsSysman = origOsSysmans[i];
    }
}

using SysmanRescanApiTest = SysmanDeviceFixture;

TEST_F(SysmanRescanApiTest, GivenSysmanOnlyInitWhenCallingRescanEntrypointThenDriverHandleIsInvoked) {
    VariableBackup<const char *> pciDevicesDirectoryBackup(&Os::pciDevicesDirectory, "/");
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    NEO::directoryFilesMap.clear();
    NEO::directoryFilesMap[Os::pciDevicesDirectory] = {};
    NEO::directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0000:03:00.0-render");

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanRescanApiTest, GivenSysmanInitFromCoreWhenCallingRescanEntrypointThenUnsupportedFeatureIsReturned) {
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, true);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, nullptr));
}

TEST_F(SysmanRescanApiTest, GivenNeitherInitFlagSetWhenCallingRescanEntrypointThenUninitializedIsReturned) {
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, false);
    VariableBackup<bool> sysmanOnlyInitBackup(&L0::Sysman::sysmanOnlyInit, false);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, nullptr));
}

class SysmanRescanEndToEndTest : public SysmanDeviceFixture {
  public:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle.get());

        pMockOsSysmanDriver = std::make_unique<MockRescanOsSysmanDriver>();
        pOrigOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
        pSysmanDriverHandleImp->pOsSysmanDriver = pMockOsSysmanDriver.get();

        pMockSysfsAccess = std::make_unique<MockRescanSysfsAccess>();
        pOrigSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

        pLinuxSysmanImp->pciUuid = pMockSysfsAccess->mockUuid;

        pMockProcfsAccess = std::make_unique<MockRescanProcfsAccess>();
        pOrigProcfsAccess = pLinuxSysmanImp->pProcfsAccess;
        pLinuxSysmanImp->pProcfsAccess = pMockProcfsAccess.get();

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;

        pciDevicesDirectoryBackup = std::make_unique<VariableBackup<const char *>>(&Os::pciDevicesDirectory, "/");
        directoryFilesMapBackup = std::make_unique<VariableBackup<std::map<std::string, std::vector<std::string>>>>(&NEO::directoryFilesMap);
        NEO::directoryFilesMap.clear();
        NEO::directoryFilesMap[Os::pciDevicesDirectory] = {};
        NEO::directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0000:03:00.0-render");
    }

    void TearDown() override {
        pLinuxSysmanImp->pProcfsAccess = pOrigProcfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pOrigSysfsAccess;
        pSysmanDriverHandleImp->pOsSysmanDriver = pOrigOsSysmanDriver;
        SysmanDeviceFixture::TearDown();
    }

    zes_pci_properties_t getPciProperties(zes_device_handle_t hDevice) {
        zes_pci_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetProperties(hDevice, &properties));
        return properties;
    }

    L0::Sysman::SysmanDriverHandleImp *pSysmanDriverHandleImp = nullptr;
    std::unique_ptr<MockRescanOsSysmanDriver> pMockOsSysmanDriver;
    std::unique_ptr<MockRescanSysfsAccess> pMockSysfsAccess;
    std::unique_ptr<MockRescanProcfsAccess> pMockProcfsAccess;
    L0::Sysman::OsSysmanDriver *pOrigOsSysmanDriver = nullptr;
    L0::Sysman::SysFsAccessInterface *pOrigSysfsAccess = nullptr;
    L0::Sysman::ProcFsAccessInterface *pOrigProcfsAccess = nullptr;
    std::unique_ptr<VariableBackup<const char *>> pciDevicesDirectoryBackup;
    std::unique_ptr<VariableBackup<std::map<std::string, std::vector<std::string>>>> directoryFilesMapBackup;
};

TEST_F(SysmanRescanEndToEndTest, GivenBdfChangesWhenCallingRescanEntrypointThenSameDeviceHandleReportsNewBdfInPciProperties) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    auto hDevice = pSysmanDevice->toHandle();

    auto propertiesBeforeRescan = getPciProperties(hDevice);
    EXPECT_EQ(0x03u, propertiesBeforeRescan.address.bus);

    std::string cachedUuid = static_cast<LinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman)->getPciUuid();
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = cachedUuid;

    pMockSysfsAccess->mockBdf = "0000:ab:00.0";

    uint32_t count = 0;
    std::vector<zes_device_handle_t> handles;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_GT(count, 0u);
    handles.resize(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, handles.data()));

    EXPECT_EQ(hDevice, handles[0]);
    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    auto propertiesAfterRescan = getPciProperties(hDevice);
    EXPECT_EQ(0xabu, propertiesAfterRescan.address.bus);
    EXPECT_NE(propertiesBeforeRescan.address.bus, propertiesAfterRescan.address.bus);
}

TEST_F(SysmanRescanEndToEndTest, GivenNullGlobalOperationsWhenCallingRescanEntrypointThenClearCachesIsSkippedAndBdfIsStillUpdated) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    auto hDevice = pSysmanDevice->toHandle();

    auto propertiesBeforeRescan = getPciProperties(hDevice);
    EXPECT_EQ(0x03u, propertiesBeforeRescan.address.bus);

    std::string cachedUuid = static_cast<LinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman)->getPciUuid();
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = cachedUuid;
    pMockSysfsAccess->mockBdf = "0000:ab:00.0";

    VariableBackup<L0::Sysman::GlobalOperations *> globalOpsBackup(&pSysmanDeviceImp->pGlobalOperations, nullptr);

    uint32_t count = 0;
    std::vector<zes_device_handle_t> handles;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_GT(count, 0u);
    handles.resize(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, handles.data()));

    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    auto propertiesAfterRescan = getPciProperties(hDevice);
    EXPECT_EQ(0xabu, propertiesAfterRescan.address.bus);
    EXPECT_NE(propertiesBeforeRescan.address.bus, propertiesAfterRescan.address.bus);
}

TEST_F(SysmanRescanEndToEndTest, GivenAllModulesInitializedWhenCallingRescanEntrypointThenReInitSysmanDeviceCacheReInitsEachInitializedModule) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    auto hDevice = pSysmanDevice->toHandle();

    uint32_t count = 0;
    zesDeviceEnumEngineGroups(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumPowerDomains(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumFrequencyDomains(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumTemperatureSensors(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumFans(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumStandbyDomains(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumDiagnosticTestSuites(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumFirmwares(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumRasErrorSets(hDevice, &count, nullptr);
    count = 0;
    zesDeviceEnumEnabledVFExp(hDevice, &count, nullptr);

    EXPECT_TRUE(pSysmanDeviceImp->pEngineHandleContext->isEngineInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pFrequencyHandleContext->isFrequencyInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pFanHandleContext->isFanInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pStandbyHandleContext->isStandbyInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pTempHandleContext->isTempInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pPowerHandleContext->isPowerInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pDiagnosticsHandleContext->isDiagnosticsInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pFirmwareHandleContext->isFirmwareInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pRasHandleContext->isRasInitDone());
    EXPECT_TRUE(pSysmanDeviceImp->pVfManagementHandleContext->isVfManagementInitDone());

    std::string cachedUuid = static_cast<LinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman)->getPciUuid();
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = cachedUuid;
    pMockSysfsAccess->mockBdf = "0000:ab:00.0";

    count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverRescanDevicesExp(driverHandle->toHandle(), &count, handles.data()));
    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

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

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDriverHandleWithNoDevicesWhenCallingRescanDevicesThenUninitializedIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    // A driver handle with no sysman devices must be rejected before sysmanDevices[0] is dereferenced.
    auto emptyDriverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
    ASSERT_TRUE(emptyDriverHandle->sysmanDevices.empty());

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pLinuxDriverImp->rescanDevices(emptyDriverHandle.get(), &count, nullptr));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenNonDrmHwDeviceIdWhenCallingGetPciBdfAndUuidForHwDeviceThenErrorUnknownIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    class NonDrmHwDeviceId : public NEO::HwDeviceId {
      public:
        NonDrmHwDeviceId() : NEO::HwDeviceId(NEO::DriverModelType::wddm) {}
    };
    NonDrmHwDeviceId hwDeviceId;

    std::string pciBdf;
    std::string pciUuid;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxDriverImp->getPciBdfAndUuidForHwDevice(&hwDeviceId, pciBdf, pciUuid));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndValidUuidFileWhenCallingGetPciBdfAndUuidForHwDeviceThenBdfAndUuidAreReturned) {
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciBdfAndUuidForHwDevice(&hwDeviceId, pciBdf, pciUuid));
    EXPECT_EQ(mockPciPath, pciBdf);
    EXPECT_EQ(mockUuidValue, std::string(pciUuid.c_str()));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndShortUuidFileWhenCallingGetPciBdfAndUuidForHwDeviceThenReturnedUuidIsResizedToBytesReadWithoutNullPadding) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:03:00.0");

    // 36 UUID chars plus a trailing '\n'; the newline is converted to an embedded null by the
    // std::replace, so the read length (37) must still be reflected in the resized string.
    constexpr size_t expectedBytesRead = sizeof("01234567-89ab-cdef-0123-456789abcdef\n") - 1;

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

    std::string pciBdf;
    std::string pciUuid;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciBdfAndUuidForHwDevice(&hwDeviceId, pciBdf, pciUuid));

    // Resized to bytesRead: no trailing null padding from the 64-byte scratch buffer.
    EXPECT_EQ(expectedBytesRead, pciUuid.size());
    EXPECT_EQ(expectedBytesRead, std::string(pciUuid.c_str()).size() + 1);
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndUuidFileOpenFailsWhenCallingGetPciBdfAndUuidForHwDeviceThenErrorIsReturnedAndUuidIsEmpty) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    NEO::HwDeviceIdDrm hwDeviceId(-1, "0000:03:00.0");

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });

    std::string pciBdf;
    std::string pciUuid;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxDriverImp->getPciBdfAndUuidForHwDevice(&hwDeviceId, pciBdf, pciUuid));
    EXPECT_TRUE(pciUuid.empty());
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDrmHwDeviceIdAndUuidFileReadFailsWhenCallingGetPciBdfAndUuidForHwDeviceThenErrorIsReturnedAndUuidIsEmpty) {
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

    std::string pciBdf;
    std::string pciUuid;
    EXPECT_NE(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciBdfAndUuidForHwDevice(&hwDeviceId, pciBdf, pciUuid));
    EXPECT_TRUE(pciUuid.empty());
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenNonDrmDriverModelWhenCallingUpdateHwDeviceIdThenUnsupportedFeatureIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    pSysmanDeviceImp->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>(NEO::DriverModelType::wddm));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, "0000:ab:00.0"));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenNoDeviceDiscoveredAtNewBdfWhenCallingUpdateHwDeviceIdThenDeviceLostIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    VariableBackup<const char *> pciDevicesDirectoryBackup(&Os::pciDevicesDirectory, "/");
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    NEO::directoryFilesMap.clear();
    NEO::directoryFilesMap[Os::pciDevicesDirectory] = {};

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, "0000:ab:00.0"));
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenDeviceDiscoveredAtNewBdfWhenCallingUpdateHwDeviceIdThenSuccessIsReturnedAndDrmBdfIsUpdated) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    const std::string newBdf = "0000:ab:00.0";

    VariableBackup<const char *> pciDevicesDirectoryBackup(&Os::pciDevicesDirectory, "/");
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    NEO::directoryFilesMap.clear();
    NEO::directoryFilesMap[Os::pciDevicesDirectory] = {};
    NEO::directoryFilesMap[Os::pciDevicesDirectory].push_back(std::string("/pci-") + newBdf + "-render");

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, newBdf));

    auto pciBusInfo = pLinuxSysmanImp->getDrm()->getPciBusInfo();
    EXPECT_EQ(0xabu, pciBusInfo.pciBus);
    EXPECT_EQ(0x0u, pciBusInfo.pciDevice);
    EXPECT_EQ(0x0u, pciBusInfo.pciFunction);
}

TEST_F(SysmanRescanLinuxDriverImpTest, GivenQueryAdapterBdfFailsWhenCallingUpdateHwDeviceIdThenUninitializedIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    const std::string newBdf = "0000:ab:00.0";

    VariableBackup<const char *> pciDevicesDirectoryBackup(&Os::pciDevicesDirectory, "/");
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    NEO::directoryFilesMap.clear();
    NEO::directoryFilesMap[Os::pciDevicesDirectory] = {};
    NEO::directoryFilesMap[Os::pciDevicesDirectory].push_back(std::string("/pci-") + newBdf + "-render");

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });

    auto pMockDrm = new MockRescanDrm(pSysmanDeviceImp->getRootDeviceEnvironmentRef());
    pMockDrm->mockQueryAdapterBdfResult = 1;
    pSysmanDeviceImp->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(pMockDrm));

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pLinuxDriverImp->updateHwDeviceId(pSysmanDevice, newBdf));
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
    // On getFileName() failure the function returns early, so the sysfs path is never refreshed.
    EXPECT_EQ(0u, pMockSysfsAccess->getRealPathCalled);

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

        pSysmanDriverHandleImp->pciUuidToPciBusInfoMap[mockRescanPciUuid] = std::make_unique<NEO::PhysicalDevicePciBusInfo>(0, 0x03, 0, 0);
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

    // Discovered uuid is not in the cache map, so no relocation happens.
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = "uuid-not-in-map";

    // count == 0 with a non-null buffer: the (*pCount == 0 && phDevices == nullptr) guard is false
    // because phDevices != nullptr, so the discovery loop runs and getDevice() reports the count.
    uint32_t count = 0;
    zes_device_handle_t handle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, &handle));
    EXPECT_EQ(1u, count);
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

TEST_F(SysmanRescanDriverHandleTest, GivenGetPciBdfAndUuidFailsWhenCallingGetDeviceRescanThenErrorIsPropagated) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    pMockOsSysmanDriver->mockGetPciBdfAndUuidResult = ZE_RESULT_ERROR_UNKNOWN;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
}

TEST_F(SysmanRescanDriverHandleTest, GivenUnparsableBdfWhenCallingGetDeviceRescanThenDependencyUnavailableIsReturnedAndDeviceIsNotRelocated) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    pMockOsSysmanDriver->mockPciBdf = "bad-bdf";

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, nullptr));
    EXPECT_EQ(1u, count);

    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

TEST_F(SysmanRescanDriverHandleTest, GivenUuidNotInCacheMapWhenCallingGetDeviceRescanThenDeviceIsSkipped) {
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

TEST_F(SysmanRescanDriverHandleTest, GivenNullCachedBusInfoForDiscoveredUuidWhenCallingGetDeviceRescanThenDeviceIsSkipped) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&NEO::directoryFilesMap);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    // Discovered uuid is present in the cache map but its bus info is null, so the null-guard skips
    // the device and no relocation is attempted.
    pSysmanDriverHandleImp->pciUuidToPciBusInfoMap[mockRescanPciUuid] = nullptr;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);
}

TEST_F(SysmanRescanDriverHandleTest, GivenEmptyPciUuidWhenCallingUpdatePciUuidMapThenNoEntryIsInserted) {
    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = "";
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    const size_t sizeBefore = pSysmanDriverHandleImp->pciUuidToPciBusInfoMap.size();
    pSysmanDriverHandleImp->updatePciUuidMap(pSysmanDeviceImp);
    EXPECT_EQ(sizeBefore, pSysmanDriverHandleImp->pciUuidToPciBusInfoMap.size());
    EXPECT_EQ(pSysmanDriverHandleImp->pciUuidToPciBusInfoMap.end(), pSysmanDriverHandleImp->pciUuidToPciBusInfoMap.find(""));

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

TEST_F(SysmanRescanDriverHandleTest, GivenBdfChangedForCachedUuidWhenCallingGetDeviceRescanThenUuidDeviceMapIsUpdatedWithRecomputedUuid) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:03:00.0");

    // After relocation the device reports a new (BDF-derived) device uuid. The rescan flow must
    // register this recomputed uuid in uuidDeviceMap so core-handle reverse lookup keeps working.
    const std::string recomputedUuid = "relocated-device-uuid";

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockDeviceUuids = true;
    pMockOsSysman->mockDeviceUuids = {recomputedUuid};
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    EXPECT_EQ(pSysmanDriverHandleImp->getUuidDeviceMap().find(recomputedUuid), pSysmanDriverHandleImp->getUuidDeviceMap().end());

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    const auto &uuidDeviceMap = pSysmanDriverHandleImp->getUuidDeviceMap();
    auto it = uuidDeviceMap.find(recomputedUuid);
    ASSERT_NE(it, uuidDeviceMap.end());
    EXPECT_EQ(static_cast<SysmanDevice *>(pSysmanDeviceImp), it->second);

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

TEST_F(SysmanRescanDriverHandleTest, GivenBdfUnchangedForCachedUuidWhenCallingGetDeviceRescanThenDeviceIsNotRelocated) {
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
    EXPECT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenBdfMatchingCacheOnlyPartiallyWhenCallingGetDeviceRescanThenDeviceIsRelocated) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = mockRescanPciUuid;
    pMockOsSysman->useMockPciBdfInfo = true;
    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo(0, 0xab, 0, 0);
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    // Relocate to a known BDF so the cache map holds (domain=0, bus=0xab, device=0, function=0).
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    setupSingleDiscoveredDevice("0000:ab:00.0");
    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    ASSERT_EQ(1u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    // BDF differs only in function: exercises the function-equality term of the compound check.
    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo(0, 0xab, 0, 1);
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.1";
    setupSingleDiscoveredDevice("0000:ab:00.1");
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(2u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    // BDF differs only in device: exercises the device-equality term of the compound check.
    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo(0, 0xab, 1, 1);
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:01.1";
    setupSingleDiscoveredDevice("0000:ab:01.1");
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(3u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    // BDF differs only in domain: exercises the domain-equality term of the compound check.
    pMockOsSysman->mockPciBdfInfo = NEO::PhysicalDevicePciBusInfo(1, 0xab, 1, 1);
    pMockOsSysmanDriver->mockPciBdf = "0001:ab:01.1";
    setupSingleDiscoveredDevice("0001:ab:01.1");
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(4u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenCachedUuidButNoDeviceMatchesWhenCallingGetDeviceRescanThenErrorUnknownIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    auto pMockOsSysman = std::make_unique<MockRescanOsSysman>(pSysmanDeviceImp);
    pMockOsSysman->mockPciUuid = "some-other-uuid";
    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = pMockOsSysman.get();

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
}

TEST_F(SysmanRescanDriverHandleTest, GivenNullOsSysmanOnDeviceWhenCallingGetDeviceRescanThenErrorUnknownIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return NEO::SysCalls::fakeFileDescriptor;
    });
    setupSingleDiscoveredDevice("0000:ab:00.0");

    auto pOrigOsSysman = pSysmanDeviceImp->pOsSysman;
    pSysmanDeviceImp->pOsSysman = nullptr;

    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = mockRescanPciUuid;

    uint32_t count = 1;
    std::vector<zes_device_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanDriverHandleImp->getDeviceRescan(&count, handles.data()));
    EXPECT_EQ(0u, pMockOsSysmanDriver->updateHwDeviceIdCalled);

    pSysmanDeviceImp->pOsSysman = pOrigOsSysman;
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

        pSysmanDriverHandleImp->pciUuidToPciBusInfoMap[pMockSysfsAccess->mockUuid] = std::make_unique<NEO::PhysicalDevicePciBusInfo>(0, 0x03, 0, 0);

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

    std::string cachedUuid = pSysmanDeviceImp->pOsSysman->getPciUuid();
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

    std::string cachedUuid = pSysmanDeviceImp->pOsSysman->getPciUuid();
    pMockOsSysmanDriver->mockPciBdf = "0000:ab:00.0";
    pMockOsSysmanDriver->mockPciUuid = cachedUuid;
    pMockSysfsAccess->mockBdf = "0000:ab:00.0";

    // Null the global operations so reInitSysmanDeviceCache() takes the pGlobalOperations == nullptr
    // branch and skips clearCaches(); the rest of the rescan flow must still complete.
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

    // Enumerate every module so each handle-context marks itself init-done. This makes
    // reInitSysmanDeviceCache() take the true side of every isXxxInitDone() guard during rescan.
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

    std::string cachedUuid = pSysmanDeviceImp->pOsSysman->getPciUuid();
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

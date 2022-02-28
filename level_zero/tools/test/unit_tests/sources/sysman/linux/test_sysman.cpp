/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

inline static int mockAccessFailure(const char *pathname, int mode) {
    return -1;
}

inline static int mockAccessSuccess(const char *pathname, int mode) {
    return 0;
}

inline static int mockStatFailure(const char *pathname, struct stat *sb) noexcept {
    return -1;
}

inline static int mockStatSuccess(const char *pathname, struct stat *sb) noexcept {
    sb->st_mode = S_IWUSR | S_IRUSR;
    return 0;
}

inline static int mockStatNoPermissions(const char *pathname, struct stat *sb) noexcept {
    sb->st_mode = 0;
    return 0;
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleInSysmanImpCreationWhenAllSysmanInterfacesAreAssignedToNullThenExpectSysmanDeviceModuleContextsAreNull) {
    ze_device_handle_t hSysman = device->toHandle();
    SysmanDeviceImp *sysmanImp = new SysmanDeviceImp(hSysman);

    delete (sysmanImp->pPowerHandleContext);
    delete (sysmanImp->pFrequencyHandleContext);
    delete (sysmanImp->pFabricPortHandleContext);
    delete (sysmanImp->pTempHandleContext);
    delete (sysmanImp->pPci);
    delete (sysmanImp->pStandbyHandleContext);
    delete (sysmanImp->pEngineHandleContext);
    delete (sysmanImp->pSchedulerHandleContext);
    delete (sysmanImp->pRasHandleContext);
    delete (sysmanImp->pMemoryHandleContext);
    delete (sysmanImp->pGlobalOperations);
    delete (sysmanImp->pEvents);
    delete (sysmanImp->pFanHandleContext);
    delete (sysmanImp->pFirmwareHandleContext);
    delete (sysmanImp->pDiagnosticsHandleContext);
    delete (sysmanImp->pPerformanceHandleContext);

    sysmanImp->pPowerHandleContext = nullptr;
    sysmanImp->pFrequencyHandleContext = nullptr;
    sysmanImp->pFabricPortHandleContext = nullptr;
    sysmanImp->pTempHandleContext = nullptr;
    sysmanImp->pPci = nullptr;
    sysmanImp->pStandbyHandleContext = nullptr;
    sysmanImp->pEngineHandleContext = nullptr;
    sysmanImp->pSchedulerHandleContext = nullptr;
    sysmanImp->pRasHandleContext = nullptr;
    sysmanImp->pMemoryHandleContext = nullptr;
    sysmanImp->pGlobalOperations = nullptr;
    sysmanImp->pEvents = nullptr;
    sysmanImp->pFanHandleContext = nullptr;
    sysmanImp->pFirmwareHandleContext = nullptr;
    sysmanImp->pDiagnosticsHandleContext = nullptr;
    sysmanImp->pPerformanceHandleContext = nullptr;

    auto pLinuxSysmanImpTemp = static_cast<PublicLinuxSysmanImp *>(sysmanImp->pOsSysman);
    pLinuxSysmanImpTemp->pSysfsAccess = pSysfsAccess;
    pLinuxSysmanImpTemp->pProcfsAccess = pProcfsAccess;

    sysmanImp->init();
    // all sysman module contexts are null. Validating PowerHandleContext instead of all contexts
    EXPECT_EQ(sysmanImp->pPowerHandleContext, nullptr);
    pLinuxSysmanImpTemp->pSysfsAccess = nullptr;
    pLinuxSysmanImpTemp->pProcfsAccess = nullptr;
    delete sysmanImp;
    sysmanImp = nullptr;
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleAndIfSysmanDeviceInitFailsThenErrorReturnedWhileQueryingSysmanAPIs) {
    ze_device_handle_t hSysman = device->toHandle();
    auto pSysmanDeviceOriginal = static_cast<DeviceImp *>(device)->getSysmanHandle();

    // L0::SysmanDeviceHandleContext::init() would return nullptr as:
    // L0::SysmanDeviceHandleContext::init() --> sysmanDevice->init() --> pOsSysman->init() --> pSysfsAccess->getRealPath()
    // pSysfsAccess->getRealPath() would fail because pSysfsAccess is not mocked in this test case.
    auto pSysmanDeviceLocal = L0::SysmanDeviceHandleContext::init(hSysman);
    EXPECT_EQ(pSysmanDeviceLocal, nullptr);
    static_cast<DeviceImp *>(device)->setSysmanHandle(pSysmanDeviceLocal);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumSchedulers(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceProcessesGetState(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDevicePciGetBars(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumPowerDomains(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumFrequencyDomains(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumEngineGroups(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumStandbyDomains(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumFirmwares(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumMemoryModules(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumFabricPorts(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumTemperatureSensors(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumRasErrorSets(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumFans(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumDiagnosticTestSuites(hSysman, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEnumPerformanceFactorDomains(hSysman, &count, nullptr));

    zes_device_properties_t properties;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceGetProperties(hSysman, &properties));
    zes_device_state_t state;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceGetState(hSysman, &state));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceReset(hSysman, true));
    zes_pci_properties_t pciProperties;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDevicePciGetProperties(hSysman, &pciProperties));
    zes_pci_state_t pciState;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDevicePciGetState(hSysman, &pciState));
    zes_pci_stats_t pciStats;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDevicePciGetStats(hSysman, &pciStats));
    zes_event_type_flags_t events = ZES_EVENT_TYPE_FLAG_DEVICE_DETACH;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEventRegister(hSysman, events));
    static_cast<DeviceImp *>(device)->setSysmanHandle(pSysmanDeviceOriginal);
}

using MockDeviceSysmanGetTest = Test<DeviceFixture>;
TEST_F(MockDeviceSysmanGetTest, GivenValidSysmanHandleSetInDeviceStructWhenGetThisSysmanHandleThenHandlesShouldBeSimilar) {
    SysmanDeviceImp *sysman = new SysmanDeviceImp(device->toHandle());
    device->setSysmanHandle(sysman);
    EXPECT_EQ(sysman, device->getSysmanHandle());
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleButSysmanInitFailsThenValidNullptrReceived) {
    ze_device_handle_t hSysman = device->toHandle();
    auto pSysmanDevice = L0::SysmanDeviceHandleContext::init(hSysman);
    EXPECT_EQ(pSysmanDevice, nullptr);
}

TEST_F(SysmanDeviceFixture, GivenSetValidDrmHandleForDeviceWhenDoingOsSysmanDeviceInitThenSameDrmHandleIsRetrieved) {
    EXPECT_EQ(&pLinuxSysmanImp->getDrm(), device->getOsInterface().getDriverModel()->as<Drm>());
}

TEST_F(SysmanDeviceFixture, GivenCreateFsAccessHandleWhenCallinggetFsAccessThenCreatedFsAccessHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pFsAccess != nullptr) {
        //delete previously allocated pFsAccess
        delete pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = nullptr;
    }
    pLinuxSysmanImp->pFsAccess = FsAccess::create();
    EXPECT_EQ(&pLinuxSysmanImp->getFsAccess(), pLinuxSysmanImp->pFsAccess);
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingDirectoryExistsWithValidAndInvalidPathThenSuccessAndFailureAreReturnedRespectively) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->accessSyscall = mockAccessSuccess;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_TRUE(tempFsAccess->directoryExists(path));
    tempFsAccess->accessSyscall = mockAccessFailure;
    path = "invalidDiretory";
    EXPECT_FALSE(tempFsAccess->directoryExists(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicSysfsAccessClassWhenCallingDirectoryExistsWithInvalidPathThenFalseIsRetured) {
    PublicFsAccess *tempSysfsAccess = new PublicFsAccess();
    tempSysfsAccess->accessSyscall = mockAccessFailure;
    std::string path = "invalidDiretory";
    EXPECT_FALSE(tempSysfsAccess->directoryExists(path));
    delete tempSysfsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithUserHavingWritePermissionsThenSuccessIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatSuccess;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->canWrite(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithUserHavingReadPermissionsThenSuccessIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatSuccess;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithUserNotHavingWritePermissionsThenInsufficientIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatNoPermissions;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, tempFsAccess->canWrite(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithUserNotHavingReadPermissionsThenInsufficientIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatNoPermissions;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithInvalidPathThenErrorIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatFailure;
    std::string path = "invalidPath";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithInvalidPathThenErrorIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatFailure;
    std::string path = "invalidPath";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenValidPathnameWhenCallingFsAccessExistsThenSuccessIsReturned) {
    auto FsAccess = pLinuxSysmanImp->getFsAccess();

    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_TRUE(FsAccess.fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPathnameWhenCallingFsAccessExistsThenErrorIsReturned) {
    auto FsAccess = pLinuxSysmanImp->getFsAccess();

    std::string path = "noSuchFileOrDirectory";
    EXPECT_FALSE(FsAccess.fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenCreateSysfsAccessHandleWhenCallinggetSysfsAccessThenCreatedSysfsAccessHandleHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pSysfsAccess != nullptr) {
        //delete previously allocated pSysfsAccess
        delete pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = nullptr;
    }
    pLinuxSysmanImp->pSysfsAccess = SysfsAccess::create("");
    EXPECT_EQ(&pLinuxSysmanImp->getSysfsAccess(), pLinuxSysmanImp->pSysfsAccess);
}

TEST_F(SysmanDeviceFixture, GivenCreateProcfsAccessHandleWhenCallinggetProcfsAccessThenCreatedProcfsAccessHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pProcfsAccess != nullptr) {
        //delete previously allocated pProcfsAccess
        delete pLinuxSysmanImp->pProcfsAccess;
        pLinuxSysmanImp->pProcfsAccess = nullptr;
    }
    pLinuxSysmanImp->pProcfsAccess = ProcfsAccess::create();
    EXPECT_EQ(&pLinuxSysmanImp->getProcfsAccess(), pLinuxSysmanImp->pProcfsAccess);
}

TEST_F(SysmanDeviceFixture, GivenValidPidWhenCallingProcfsAccessIsAliveThenSuccessIsReturned) {
    auto ProcfsAccess = pLinuxSysmanImp->getProcfsAccess();

    EXPECT_TRUE(ProcfsAccess.isAlive(getpid()));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPidWhenCallingProcfsAccessIsAliveThenErrorIsReturned) {
    auto ProcfsAccess = pLinuxSysmanImp->getProcfsAccess();

    EXPECT_FALSE(ProcfsAccess.isAlive(reinterpret_cast<::pid_t>(-1)));
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleThenSameHandleIsRetrievedFromOsSpecificCode) {
    EXPECT_EQ(pLinuxSysmanImp->getDeviceHandle(), device);
}

TEST_F(SysmanDeviceFixture, GivenPmuInterfaceHandleWhenCallinggetPmuInterfaceThenCreatedPmuInterfaceHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pPmuInterface != nullptr) {
        //delete previously allocated pPmuInterface
        delete pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = nullptr;
    }
    pLinuxSysmanImp->pPmuInterface = PmuInterface::create(pLinuxSysmanImp);
    EXPECT_EQ(pLinuxSysmanImp->getPmuInterface(), pLinuxSysmanImp->pPmuInterface);
}

TEST_F(SysmanDeviceFixture, GivenValidPciPathWhileGettingRootPciPortThenReturnedPathIs2LevelUpThenTheCurrentPath) {
    const std::string mockBdf = "0000:00:02.0";
    const std::string mockRealPath = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/" + mockBdf;
    const std::string mockRealPath2LevelsUp = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0";

    std::string pciRootPort1 = pLinuxSysmanImp->getPciRootPortDirectoryPath(mockRealPath);
    EXPECT_EQ(pciRootPort1, mockRealPath2LevelsUp);

    std::string pciRootPort2 = pLinuxSysmanImp->getPciRootPortDirectoryPath("device");
    EXPECT_EQ(pciRootPort2, "device");
}

TEST_F(SysmanDeviceFixture, GivenNullDrmHandleWhenGettingDrmHandleThenValidDrmHandleIsReturned) {
    pLinuxSysmanImp->releaseLocalDrmHandle();
    EXPECT_NO_THROW(pLinuxSysmanImp->getDrm());
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenProductFamilyFromDeviceThenValidCorrectProductFamilyIsReturned) {
    auto productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    EXPECT_EQ(productFamily, pLinuxSysmanImp->getProductFamily());
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDeviceHandleHavingSubdevicesWhenValidatingSysmanHandlesForSubdevicesThenSysmanHandleForSubdeviceWillBeSameAsSysmanHandleForDevice) {
    ze_device_handle_t hSysman = device->toHandle();
    auto pSysmanDeviceOriginal = static_cast<DeviceImp *>(device)->getSysmanHandle();
    auto pSysmanDeviceLocal = L0::SysmanDeviceHandleContext::init(hSysman);
    EXPECT_EQ(pSysmanDeviceLocal, nullptr);
    static_cast<DeviceImp *>(device)->setSysmanHandle(pSysmanDeviceLocal);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&count, nullptr));
    std::vector<ze_device_handle_t> subDeviceHandles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&count, subDeviceHandles.data()));
    for (auto subDeviceHandle : subDeviceHandles) {
        L0::DeviceImp *subDeviceHandleImp = static_cast<DeviceImp *>(Device::fromHandle(subDeviceHandle));
        EXPECT_EQ(subDeviceHandleImp->getSysmanHandle(), device->getSysmanHandle());
    }
    static_cast<DeviceImp *>(device)->setSysmanHandle(pSysmanDeviceOriginal);
}

TEST_F(SysmanMultiDeviceFixture, GivenValidEffectiveUserIdCheckWhetherPermissionsReturnedByIsRootUserAreCorrect) {
    int euid = geteuid();
    auto pFsAccess = pLinuxSysmanImp->getFsAccess();
    if (euid == 0) {
        EXPECT_EQ(true, pFsAccess.isRootUser());
    } else {
        EXPECT_EQ(false, pFsAccess.isRootUser());
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenSysmanEnvironmentVariableSetWhenCreateL0DeviceThenSysmanHandleCreateIsAttempted) {
    driverHandle->enableSysman = true;
    // In SetUp of SysmanMultiDeviceFixture, sysman handle for device is already created, so new sysman handle should not be created
    static_cast<DeviceImp *>(device)->createSysmanHandle(true);
    EXPECT_EQ(device->getSysmanHandle(), pSysmanDevice);

    static_cast<DeviceImp *>(device)->createSysmanHandle(false);
    EXPECT_EQ(device->getSysmanHandle(), pSysmanDevice);

    // delete previously allocated sysman handle and then attempt to create sysman handle again
    delete pSysmanDevice;
    device->setSysmanHandle(nullptr);
    static_cast<DeviceImp *>(device)->createSysmanHandle(true);
    EXPECT_EQ(device->getSysmanHandle(), nullptr);

    static_cast<DeviceImp *>(device)->createSysmanHandle(false);
    EXPECT_EQ(device->getSysmanHandle(), nullptr);
}

class UnknownDriverModel : public DriverModel {
  public:
    UnknownDriverModel() : DriverModel(DriverModelType::UNKNOWN) {}
    void setGmmInputArgs(void *args) override {}
    uint32_t getDeviceHandle() const override { return 0u; }
    PhysicalDevicePciBusInfo getPciBusInfo() const override {
        PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue);
        return pciBusInfo;
    }
    PhyicalDevicePciSpeedInfo getPciSpeedInfo() const override { return {}; }

    bool isGpuHangDetected(OsContext &osContext) override {
        return false;
    }
};

using SysmanUnknownDriverModelTest = Test<DeviceFixture>;
TEST_F(SysmanUnknownDriverModelTest, GivenDriverModelTypeIsNotDrmWhenExecutingSysmanOnLinuxThenErrorIsReturned) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
    auto &osInterface = device->getOsInterface();
    osInterface.setDriverModel(std::make_unique<UnknownDriverModel>());
    auto pSysmanDeviceImp = std::make_unique<SysmanDeviceImp>(device->toHandle());
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxSysmanImp->init());
}

} // namespace ult
} // namespace L0

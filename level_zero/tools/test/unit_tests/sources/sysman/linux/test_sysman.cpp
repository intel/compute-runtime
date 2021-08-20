/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

inline static int mockAccessFailure(const char *pathname, int mode) {
    return -1;
}

inline static int mockAccessSuccess(const char *pathname, int mode) {
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

    sysmanImp->init();
    // all sysman module contexts are null. Validating PowerHandleContext instead of all contexts
    EXPECT_EQ(sysmanImp->pPowerHandleContext, nullptr);
    delete sysmanImp;
    sysmanImp = nullptr;
}

using MockDeviceSysmanGetTest = Test<DeviceFixture>;
TEST_F(MockDeviceSysmanGetTest, GivenValidSysmanHandleSetInDeviceStructWhenGetThisSysmanHandleThenHandlesShouldBeSimilar) {
    SysmanDeviceImp *sysman = new SysmanDeviceImp(device->toHandle());
    device->setSysmanHandle(sysman);
    EXPECT_EQ(sysman, device->getSysmanHandle());
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleInSysmanInitThenValidSysmanHandleReceived) {
    ze_device_handle_t hSysman = device->toHandle();
    auto pSysmanDevice = L0::SysmanDeviceHandleContext::init(hSysman);
    EXPECT_NE(pSysmanDevice, nullptr);
    delete pSysmanDevice;
    pSysmanDevice = nullptr;
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

TEST_F(SysmanDeviceFixture, GivenXmlParserHandleWhenCallinggetXmlParserThenCreatedXmlParserHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pXmlParser != nullptr) {
        //delete previously allocated XmlParser
        delete pLinuxSysmanImp->pXmlParser;
        pLinuxSysmanImp->pXmlParser = nullptr;
    }
    pLinuxSysmanImp->pXmlParser = XmlParser::create();
    EXPECT_EQ(pLinuxSysmanImp->getXmlParser(), pLinuxSysmanImp->pXmlParser);
}

TEST_F(SysmanDeviceFixture, GivenFwUtilInterfaceHandleWhenCallinggetFwUtilInterfaceThenCreatedFwUtilInterfaceHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pFwUtilInterface != nullptr) {
        //delete previously allocated FwUtilInterface
        delete pLinuxSysmanImp->pFwUtilInterface;
        pLinuxSysmanImp->pFwUtilInterface = nullptr;
    }
    pLinuxSysmanImp->pFwUtilInterface = FirmwareUtil::create();
    EXPECT_EQ(pLinuxSysmanImp->getFwUtilInterface(), pLinuxSysmanImp->pFwUtilInterface);
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

TEST_F(SysmanMultiDeviceFixture, GivenValidDeviceHandleHavingSubdevicesWhenValidatingSysmanHandlesForSubdevicesThenSysmanHandleForSubdeviceWillBeSameAsSysmanHandleForDevice) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&count, nullptr));
    std::vector<ze_device_handle_t> subDeviceHandles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&count, subDeviceHandles.data()));
    for (auto subDeviceHandle : subDeviceHandles) {
        L0::DeviceImp *subDeviceHandleImp = static_cast<DeviceImp *>(Device::fromHandle(subDeviceHandle));
        EXPECT_EQ(subDeviceHandleImp->getSysmanHandle(), device->getSysmanHandle());
    }
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

} // namespace ult
} // namespace L0

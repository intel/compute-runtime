/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_global_operations.h"

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
    std::unique_ptr<Mock<GlobalOperationsSysfsAccess>> pSysfsAccess;
    std::unique_ptr<Mock<GlobalOperationsProcfsAccess>> pProcfsAccess;
    std::unique_ptr<Mock<GlobalOperationsFsAccess>> pFsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    ProcfsAccess *pProcfsAccessOld = nullptr;
    FsAccess *pFsAccessOld = nullptr;
    OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::GlobalOperationsImp *pGlobalOperationsImp;
    std::string expectedModelName;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pProcfsAccessOld = pLinuxSysmanImp->pProcfsAccess;
        pFsAccessOld = pLinuxSysmanImp->pFsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsSysfsAccess>>>();
        pProcfsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsProcfsAccess>>>();
        pFsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsFsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDirEntries));
        ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValAgamaFile));

        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        expectedModelName = neoDevice->getDeviceName(neoDevice->getHardwareInfo());
        pGlobalOperationsImp->init();
    }

    void TearDown() override {
        if (nullptr != pGlobalOperationsImp->pOsGlobalOperations) {
            delete pGlobalOperationsImp->pOsGlobalOperations;
        }
        pGlobalOperationsImp->pOsGlobalOperations = pOsGlobalOperationsPrev;
        pGlobalOperationsImp = nullptr;
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccessOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOld;
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzetGlobalOperationsGetPropertiesThenVerifyzetGlobalOperationsGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.numSubdevices, 0u);
    EXPECT_TRUE(0 == unknown.compare(properties.boardNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.brandName));
    EXPECT_TRUE(0 == driverVersion.compare(properties.driverVersion));
    EXPECT_TRUE(0 == expectedModelName.compare(properties.modelName));
    EXPECT_TRUE(0 == unknown.compare(properties.serialNumber));
    EXPECT_TRUE(0 == vendorIntel.compare(properties.vendorName));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAgmaFileIsAbsentThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    std::string test;
    test = srcVersion;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValSrcFile));
    pGlobalOperationsImp->init();

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == test.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAgmaFileAndSrcFileIsAbsentThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;

    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValSrcFileNotAvaliable));
    pGlobalOperationsImp->init();

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileIsNotAvaliableThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValNotAvaliable));
    pGlobalOperationsImp->init();

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenDriverVersionFileReadFailsThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValErrorUnkown));
    pGlobalOperationsImp->init();

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDevicePropertiesWhenVendorIsUnKnownThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getFalseValString));
    neoDevice->deviceInfo.vendorId = 1806; //Unknown Vendor id
    pGlobalOperationsImp->init();
    zes_device_properties_t properties;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.vendorName));
    EXPECT_TRUE(0 == unknown.compare(properties.brandName));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetPropertiesForCheckingDriverVersionWhenAccessingAgamaFileOrSrcFileGotPermissionDeniedThenVerifyzesDeviceGetPropertiesCallSucceeds) {
    zes_device_properties_t properties;
    ON_CALL(*pFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValPermissionDenied));
    pGlobalOperationsImp->init();

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

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingFaultyClientFileThenFailureIsReturned) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDir4Entries));
    ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesDeviceProcessesGetState(device, &count, nullptr));
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

TEST_F(SysmanGlobalOperationsFixture, GivenPermissionDeniedWhenCallingGetDeviceStateThenZeResultErrorInsufficientPermissionsIsReturned) {
    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getPermissionDenied));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingResetThenZeResultErrorHandleObjectInUseIsReturned) {
    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceNotInUseWhenCallingResetThenSuccessIsReturned) {
    // Pretend we have the device open
    pProcfsAccess->ourDevicePid = getpid();
    pProcfsAccess->ourDeviceFd = ::open("/dev/null", 0);

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    // The first time we get the process list, include our own process, that has the file open
    // Reset should close the file (we verify after reset). On subsequent calls, return
    // the process list without our process
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::writeSuccess));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // Check that reset closed the device
    // If the device is already closed, then close will fail with errno of EBADF
    EXPECT_NE(0, ::close(pProcfsAccess->ourDevicePid));
    EXPECT_EQ(errno, EBADF);
}

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueAndDeviceInUseWhenCallingResetThenSuccessIsReturned) {
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), isAlive(_))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::writeSuccess));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), isAlive(_))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::writeSuccess));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetAndBindFailsThenFailureIsReturned) {
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), isAlive(_))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::writeSuccess));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpFailure));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetAndGetFileNameFailsThenSuccessIsReturned) {
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), isAlive(_))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileNameReturnError));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    ON_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::writeSuccess));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessWontDieWhenCallingResetThenZeResultErrorHandleObjectInUseErrorIsReturned) {
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    static_cast<PublicLinuxGlobalOperationsImp *>(pGlobalOperationsImp->pOsGlobalOperations)->resetTimeout = 0; // timeout immediate

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    // Return process list without open fd on first call, but with open fd on subsequent calls
    EXPECT_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceUnused))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), isAlive(_))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    ON_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileName));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    EXPECT_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .Times(1);
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .Times(0);
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .Times(0);
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetAndGetFileDescriptorsFailsThenSuccessIsReturned) {
    // Pretend another process has the device open
    pProcfsAccess->ourDevicePid = getpid() + 1; // make sure it isn't our process id
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;

    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(mockFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getSuccess));
    // Return process list without open fd on first call, but with open fd on subsequent calls
    ON_CALL(*pProcfsAccess.get(), listProcesses(Matcher<std::vector<::pid_t> &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockProcessListDeviceInUse));
    ON_CALL(*pProcfsAccess.get(), isAlive(_))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockIsAlive));
    ON_CALL(*pProcfsAccess.get(), myProcessId())
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockMyProcessId));
    EXPECT_CALL(*pProcfsAccess.get(), getFileDescriptors(_, Matcher<std::vector<int> &>(_)))
        .WillOnce(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptorsFailure))
        .WillRepeatedly(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileDescriptors));
    ON_CALL(*pProcfsAccess.get(), getFileName(_, _, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::getMockFileNameReturnError));
    ON_CALL(*pSysfsAccess.get(), isMyDeviceFile(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockIsMyDeviceFile));
    ON_CALL(*pProcfsAccess.get(), kill(pProcfsAccess->ourDevicePid))
        .WillByDefault(::testing::Invoke(pProcfsAccess.get(), &Mock<GlobalOperationsProcfsAccess>::mockKill));
    EXPECT_CALL(*pSysfsAccess.get(), unbindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    EXPECT_CALL(*pFsAccess.get(), write(mockFunctionResetPath, std::string("1")))
        .WillOnce(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::writeSuccess));
    EXPECT_CALL(*pSysfsAccess.get(), bindDevice(_))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::mockDeviceOpSuccess));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0

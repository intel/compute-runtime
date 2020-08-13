/*
 * Copyright (C) 2020 Intel Corporation
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
constexpr uint64_t sharedMemSize1 = 1024;
constexpr uint64_t sharedMemSize2 = 512;
// In mock function getValUnsignedLong, we have set the engines used as 0, 3 and 1.
// Hence, expecting 28 as engine field because 28 in binary would be 00011100
// This indicates bit number 2, 3 and 4 are set, thus this indicates, this process
// used ZES_ENGINE_TYPE_FLAG_3D, ZES_ENGINE_TYPE_FLAG_MEDIA and ZES_ENGINE_TYPE_FLAG_DMA
// Their corresponding mapping with i915 engine numbers are 0, 3 and 1 respectively.
constexpr int64_t engines1 = 28u;
// 4 in binary 0100, as 2nd bit is set, hence it indicates, process used ZES_ENGINE_TYPE_FLAG_3D
// Corresponding i915 mapped value in mocked getValUnsignedLong() is 0.
constexpr int64_t engines2 = 4u;
constexpr uint32_t totalProcessStates = 2u; // Two process States for two pids
const std::string expectedModelName("0x3ea5");
class SysmanGlobalOperationsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<GlobalOperationsSysfsAccess>> pSysfsAccess;
    std::unique_ptr<Mock<GlobalOperationsFsAccess>> pFsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOld = nullptr;
    OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::GlobalOperationsImp *pGlobalOperationsImp;
    PublicLinuxGlobalOperationsImp linuxGlobalOperationsImp;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pFsAccessOld = pLinuxSysmanImp->pFsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsSysfsAccess>>>();
        pFsAccess = std::make_unique<NiceMock<Mock<GlobalOperationsFsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getScannedDirEntries));
        ON_CALL(*pFsAccess.get(), read(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getValAgamaFile));

        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
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

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingResetVerifyPermissionDenied) {
    ON_CALL(*pSysfsAccess.get(), getRealPath(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<GlobalOperationsSysfsAccess>::getRealPathVal));
    ON_CALL(*pFsAccess.get(), canWrite(Matcher<std::string>(fullFunctionResetPath)))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<GlobalOperationsFsAccess>::getPermissionDenied));
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
}

} // namespace ult
} // namespace L0

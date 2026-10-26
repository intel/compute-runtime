/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"

#include <cstring>

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

static uint32_t mockFwUtilDeviceCloseCallCount = 0;
bool MockFwUtilOsLibrary::mockLoad = true;

static std::vector<std::string> mockLoadedLibraryNames;
static std::vector<std::string> mockValidLibraryNames;

static int mockIgscDeviceInitByDevice(struct igsc_device_handle *handle, const char *devicePath) {
    return IGSC_SUCCESS;
}

static int mockIgscDeviceIteratorCreate(struct igsc_device_iterator **iter) {
    return IGSC_SUCCESS;
}

static int mockIgscDeviceIteratorNext(struct igsc_device_iterator *iter, struct igsc_device_info *info) {
    memset(info, 0, sizeof(*info));
    strncpy(info->name, "/dev/mei0", IGSC_INFO_NAME_SIZE - 1);
    return IGSC_SUCCESS;
}

static void mockIgscDeviceIteratorDestroy(struct igsc_device_iterator *iter) {
}

static int mockIgscDeviceClose(struct igsc_device_handle *handle) {
    return IGSC_SUCCESS;
}

static int mockIgscDummyFunction() {
    return IGSC_SUCCESS;
}

static OsLibrary *mockLoadValidFwUtilLibraryByName(const OsLibraryCreateProperties &properties) {
    mockLoadedLibraryNames.push_back(properties.libraryName);
    if (std::find(mockValidLibraryNames.begin(), mockValidLibraryNames.end(), properties.libraryName) == mockValidLibraryNames.end()) {
        return nullptr;
    }
    auto osLibHandle = new MockFwUtilOsLibrary();
    auto dummyFunction = reinterpret_cast<void *>(&mockIgscDummyFunction);
    osLibHandle->funcMap["igsc_device_init_by_device_info"] = reinterpret_cast<void *>(&mockIgscDeviceInitByDevice);
    osLibHandle->funcMap["igsc_device_get_device_info"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_fw_version"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_iterator_create"] = reinterpret_cast<void *>(&mockIgscDeviceIteratorCreate);
    osLibHandle->funcMap["igsc_device_iterator_next"] = reinterpret_cast<void *>(&mockIgscDeviceIteratorNext);
    osLibHandle->funcMap["igsc_device_iterator_destroy"] = reinterpret_cast<void *>(&mockIgscDeviceIteratorDestroy);
    osLibHandle->funcMap["igsc_device_fw_update"] = dummyFunction;
    osLibHandle->funcMap["igsc_image_oprom_init"] = dummyFunction;
    osLibHandle->funcMap["igsc_image_oprom_type"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_oprom_update"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_oprom_version"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_close"] = reinterpret_cast<void *>(&mockIgscDeviceClose);
    osLibHandle->funcMap["igsc_ifr_get_status_ext"] = dummyFunction;
    osLibHandle->funcMap["igsc_ifr_run_mem_ppr_test"] = dummyFunction;
    return osLibHandle;
}

static OsLibrary *mockLoadEmptyFwUtilLibraryByName(const OsLibraryCreateProperties &properties) {
    mockLoadedLibraryNames.push_back(properties.libraryName);
    return new MockFwUtilOsLibrary();
}

class FwUtilLibraryNamesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mockLoadedLibraryNames.clear();
        mockValidLibraryNames.clear();
    }

    void TearDown() override {
        std::vector<std::string>().swap(mockLoadedLibraryNames);
        std::vector<std::string>().swap(mockValidLibraryNames);
    }

    VariableBackup<std::vector<std::string>> libraryNamesBackup{&FirmwareUtilImp::fwUtilLibraryNames, {"libigsc_mock.so.1", "libigsc_mock.so.0"}};
    VariableBackup<decltype(deviceInitByDevice)> deviceInitByDeviceBackup{&deviceInitByDevice};
    VariableBackup<decltype(deviceGetDeviceInfo)> deviceGetDeviceInfoBackup{&deviceGetDeviceInfo};
    VariableBackup<decltype(deviceGetFwVersion)> deviceGetFwVersionBackup{&deviceGetFwVersion};
    VariableBackup<decltype(deviceIteratorCreate)> deviceIteratorCreateBackup{&deviceIteratorCreate};
    VariableBackup<decltype(deviceItreatorNext)> deviceItreatorNextBackup{&deviceItreatorNext};
    VariableBackup<decltype(deviceItreatorDestroy)> deviceItreatorDestroyBackup{&deviceItreatorDestroy};
    VariableBackup<decltype(deviceFwUpdate)> deviceFwUpdateBackup{&deviceFwUpdate};
    VariableBackup<decltype(imageOpromInit)> imageOpromInitBackup{&imageOpromInit};
    VariableBackup<decltype(imageOpromType)> imageOpromTypeBackup{&imageOpromType};
    VariableBackup<decltype(deviceOpromUpdate)> deviceOpromUpdateBackup{&deviceOpromUpdate};
    VariableBackup<decltype(deviceOpromVersion)> deviceOpromVersionBackup{&deviceOpromVersion};
    VariableBackup<decltype(deviceClose)> deviceCloseBackup{&deviceClose};
    VariableBackup<decltype(deviceIfrGetStatusExt)> deviceIfrGetStatusExtBackup{&deviceIfrGetStatusExt};
    VariableBackup<decltype(deviceIfrRunMemPPRTest)> deviceIfrRunMemPPRTestBackup{&deviceIfrRunMemPPRTest};
};

TEST(FwUtilDeleteTest, GivenLibraryWasNotSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsNotAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceClose)> mockDeviceClose(&deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 0u);
}

TEST(FwUtilDeleteTest, GivenLibraryWasSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceClose)> mockDeviceClose(&deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
    // Prepare dummy OsLibrary for library, since no access is expected
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 1u);
}

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndGetProcAddressFailsThenFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndIgscDeviceIterCreateFailsThenFirmwareUtilInterfaceIsNotCreated) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST(FwUtilTest, GivenLibraryWasNotSetWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsNotCreated) {
    L0::ult::MockFwUtilOsLibrary::mockLoad = false;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST_F(FwUtilLibraryNamesTest, GivenNoLibraryCanBeLoadedWhenCreatingFirmwareUtilInterfaceThenAllLibraryNamesAreTriedInOrderAndFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, FirmwareUtilImp::fwUtilLibraryNames);
}

TEST_F(FwUtilLibraryNamesTest, GivenFirstLibraryCanBeLoadedWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsCreatedWithoutTryingOtherLibraries) {
    mockValidLibraryNames = {"libigsc_mock.so.1", "libigsc_mock.so.0"};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    ASSERT_NE(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, std::vector<std::string>({"libigsc_mock.so.1"}));
    delete pFwUtil;
}

TEST_F(FwUtilLibraryNamesTest, GivenOnlySecondLibraryCanBeLoadedWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsCreatedUsingSecondLibrary) {
    mockValidLibraryNames = {"libigsc_mock.so.0"};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    ASSERT_NE(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, FirmwareUtilImp::fwUtilLibraryNames);
    delete pFwUtil;
}

TEST_F(FwUtilLibraryNamesTest, GivenFirstLibraryLoadsButEntryPointsAreMissingWhenCreatingFirmwareUtilInterfaceThenOtherLibrariesAreNotTriedAndFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadEmptyFwUtilLibraryByName};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, std::vector<std::string>({"libigsc_mock.so.1"}));
}

TEST_F(FwUtilLibraryNamesTest, GivenEmptyLibraryNamesListWhenCreatingFirmwareUtilInterfaceThenNoLibraryIsLoadedAndFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<std::vector<std::string>> emptyLibraryNamesBackup{&FirmwareUtilImp::fwUtilLibraryNames, {}};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
    EXPECT_TRUE(mockLoadedLibraryNames.empty());
}

} // namespace ult
} // namespace L0

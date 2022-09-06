/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

static uint32_t mockFwUtilDeviceCloseCallCount = 0;
bool MockFwUtilOsLibrary::mockLoad = true;
bool MockFwUtilOsLibrary::getNonNullProcAddr = false;

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
    FirmwareUtilImp::osLibraryLoadFunction = L0::ult::MockFwUtilOsLibrary::load;
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndIgscDeviceIterCreateFailsThenFirmwareUtilInterfaceIsNotCreated) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    L0::ult::MockFwUtilOsLibrary::getNonNullProcAddr = false;
    FirmwareUtilImp::osLibraryLoadFunction = L0::ult::MockFwUtilOsLibrary::load;
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST(FwUtilTest, GivenLibraryWasNotSetWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsNotCreated) {
    L0::ult::MockFwUtilOsLibrary::mockLoad = false;
    FirmwareUtilImp::osLibraryLoadFunction = L0::ult::MockFwUtilOsLibrary::load;
    FirmwareUtil *pFwUtil = FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

} // namespace ult
} // namespace L0

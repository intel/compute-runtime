/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware_util/mock_fw_util_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

static uint32_t mockFwUtilDeviceCloseCallCount = 0;
bool MockFwUtilOsLibrary::mockLoad = true;

TEST(FwUtilDeleteTest, GivenLibraryWasNotSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsNotAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    VariableBackup<decltype(L0::Sysman::deviceClose)> mockDeviceClose(&L0::Sysman::deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 0u);
}

TEST(FwUtilDeleteTest, GivenLibraryWasSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    VariableBackup<decltype(L0::Sysman::deviceClose)> mockDeviceClose(&L0::Sysman::deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    // Prepare dummy OsLibrary for library, since no access is expected
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 1u);
}

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndGetProcAddressFailsThenFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndIgscDeviceIterCreateFailsThenFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST(FwUtilTest, GivenLibraryWasNotSetWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsNotCreated) {
    L0::Sysman::ult::MockFwUtilOsLibrary::mockLoad = false;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

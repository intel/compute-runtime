/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

static uint32_t mockFwUtilDeviceCloseCallCount = 0;

TEST(LinuxFwUtilDeleteTest, GivenLibraryWasNotSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsNotAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceClose)> mockDeviceClose(&deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    std::string pciBdf("0000:00:00.0");
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(pciBdf);
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 0u);
}

TEST(LinuxFwUtilDeleteTest, GivenLibraryWasSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceClose)> mockDeviceClose(&deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    std::string pciBdf("0000:00:00.0");
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(pciBdf);
    // Prepare dummy OsLibrary for library, since no access is expected
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 1u);
}

} // namespace ult
} // namespace L0

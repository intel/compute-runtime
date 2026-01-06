/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanFirmwareExtendedFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFirmwareExtendedFixture, GivenLinuxFirmwareImpWhenCallingOsFirmwareFlashExtendedThenUnsupportedFeatureIsReturned) {
    // Create LinuxFirmwareImp object with Flash_Override firmware type
    auto pLinuxFirmwareImp = std::make_unique<LinuxFirmwareImp>(pOsSysman, "Flash_Override");

    // Create test firmware image data
    std::vector<uint8_t> firmwareData(1024, 0xDD); // 1KB test data

    // Call osFirmwareFlashExtended and expect unsupported feature error
    ze_result_t result = pLinuxFirmwareImp->osFirmwareFlashExtended(firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
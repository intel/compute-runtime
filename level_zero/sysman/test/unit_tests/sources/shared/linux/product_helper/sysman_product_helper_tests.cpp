/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct SysmanProductHelperTest : public SysmanDeviceFixture {
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanProductHelperTest, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperTest, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsDG1) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperTest, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanProductHelperTest, GivenInvalidProductFamilyWhenCallingProductHelperCreateThenNullPtrIsReturned) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(IGFX_UNKNOWN);
    EXPECT_EQ(nullptr, pSysmanProductHelper);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
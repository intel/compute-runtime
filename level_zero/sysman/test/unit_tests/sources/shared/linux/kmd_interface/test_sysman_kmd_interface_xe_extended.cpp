/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanKmdInterfaceXeExtendedFixture : public SysmanDeviceFixture {
  public:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper()));
    }
};

TEST_F(SysmanKmdInterfaceXeExtendedFixture, GivenSysmanKmdInterfaceWhenCallingIsDeviceInFdoModeThenFalseIsReturned) {
    // Test the main implementation which simply returns false
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isDeviceInFdoMode());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
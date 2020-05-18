/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gtest/gtest.h"

using ::testing::_;

namespace L0 {
namespace ult {

class SysmanMemoryFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;

    void SetUp() override {

        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        hSysman = sysmanImp->toHandle();
    }
    void TearDown() override {
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanMemoryFixture, GivenComponentCountZeroWhenCallingZetSysmanMemoryGetThenZeroCountIsReturnedAndVerifySysmanMemoryGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanMemoryGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);

    uint32_t testcount = count + 1;

    result = zetSysmanMemoryGet(hSysman, &testcount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);
}

} // namespace ult
} // namespace L0

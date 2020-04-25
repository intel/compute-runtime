/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using SysmanGetTest = Test<DeviceFixture>;

TEST_F(SysmanGetTest, whenCallingZetSysmanGetWithoutCallingZetInitThenUnitializedIsReturned) {
    zet_sysman_handle_t hSysman;
    zet_sysman_version_t version = ZET_SYSMAN_VERSION_CURRENT;
    ze_result_t res = zetSysmanGet(device->toHandle(), version, &hSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
}

} // namespace ult
} // namespace L0
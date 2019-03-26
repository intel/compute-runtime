/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> SkylakeOnlyTest;

SKLTEST_F(SkylakeOnlyTest, shouldPassOnSkl) {
    EXPECT_EQ(IGFX_SKYLAKE, pDevice->getHardwareInfo().pPlatform->eProductFamily);
}

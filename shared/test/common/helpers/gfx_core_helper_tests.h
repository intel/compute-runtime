/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct ComputeSlmTestInput {
    uint32_t expected;
    uint32_t slmSize;
};

using GfxCoreHelperTest = Test<DeviceFixture>;

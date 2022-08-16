/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"

#include <vector>

class ProgramTests : public NEO::ClDeviceFixture,
                     public ::testing::Test,
                     public NEO::ContextFixture {

    using NEO::ContextFixture::setUp;

  public:
    void SetUp() override;
    void TearDown() override;
};

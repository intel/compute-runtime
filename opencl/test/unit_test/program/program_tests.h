/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

class ProgramTests : public NEO::ClDeviceFixture,
                     public ::testing::Test,
                     public NEO::ContextFixture {

    using NEO::ContextFixture::setUp;

  public:
    void SetUp() override;
    void TearDown() override;
};

class MinimumProgramFixture : public NEO::ContextFixture,
                              public NEO::PlatformFixture,
                              public ::testing::Test {

    using NEO::ContextFixture::setUp;
    using NEO::PlatformFixture::setUp;

  protected:
    void SetUp() override;
    void TearDown() override;

    cl_int retVal = CL_SUCCESS;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
};
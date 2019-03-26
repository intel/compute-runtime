/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

#include <vector>

extern std::vector<const char *> BinaryFileNames;
extern std::vector<const char *> SourceFileNames;
extern std::vector<const char *> BinaryForSourceFileNames;
extern std::vector<const char *> KernelNames;

class ProgramTests : public NEO::DeviceFixture,
                     public ::testing::Test,
                     public NEO::ContextFixture {

    using NEO::ContextFixture::SetUp;

  public:
    void SetUp() override;
    void TearDown() override;
};

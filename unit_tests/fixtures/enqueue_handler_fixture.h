/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

class EnqueueHandlerTest : public OCLRT::DeviceFixture,
                           public testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        context = new OCLRT::MockContext(pDevice);
    }

    void TearDown() override {
        context->decRefInternal();
        DeviceFixture::TearDown();
    }
    OCLRT::MockContext *context;
};

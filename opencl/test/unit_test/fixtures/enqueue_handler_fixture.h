/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

class EnqueueHandlerTest : public NEO::ClDeviceFixture,
                           public testing::Test {
  public:
    void SetUp() override {
        ClDeviceFixture::setUp();
        context = new NEO::MockContext(pClDevice);
    }

    void TearDown() override {
        context->decRefInternal();
        ClDeviceFixture::tearDown();
    }
    NEO::MockContext *context;
};

template <template <typename> class CsrType>
class EnqueueHandlerTestT : public EnqueueHandlerTest {
  public:
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        NEO::EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType<FamilyType>>();
        EnqueueHandlerTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        EnqueueHandlerTest::TearDown();
    }
};

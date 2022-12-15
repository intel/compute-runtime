/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
using namespace NEO;
namespace Ult {
class AggregatedSmallBuffersDg2DefaultTest : public ContextFixture,
                                             public ClDeviceFixture,
                                             public testing::Test {
  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
    }

    void TearDown() override {
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
};

DG2TEST_F(AggregatedSmallBuffersDg2DefaultTest, givenAggregatedSmallBuffersDefaultWhenCheckIfEnabledThenReturnTrue) {
    EXPECT_TRUE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
}

DG2TEST_F(AggregatedSmallBuffersDg2DefaultTest, givenAggregatedSmallBuffersDefaultAndMultiDeviceContextWhenCheckIfEnabledThenReturnFalse) {
    pContext->devices.push_back(nullptr);
    EXPECT_FALSE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    pContext->devices.pop_back();
}

} // namespace Ult

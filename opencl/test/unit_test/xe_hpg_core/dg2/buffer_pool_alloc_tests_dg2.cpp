/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
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

DG2TEST_F(AggregatedSmallBuffersDg2DefaultTest, givenDifferentFlagValuesAndSingleOrMultiDeviceContextWhenCheckIfEnabledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    // Single device context
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(-1);
        EXPECT_TRUE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        EXPECT_FALSE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
        EXPECT_TRUE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(2);
        EXPECT_TRUE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    // Multi device context
    pContext->devices.push_back(nullptr);
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(-1);
        EXPECT_FALSE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        EXPECT_FALSE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(1);
        EXPECT_FALSE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    {
        DebugManager.flags.ExperimentalSmallBufferPoolAllocator.set(2);
        EXPECT_TRUE(pContext->getBufferPoolAllocator().isAggregatedSmallBuffersEnabled(pContext));
    }
    pContext->devices.pop_back();
}

} // namespace Ult

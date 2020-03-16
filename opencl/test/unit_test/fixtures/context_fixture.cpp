/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/context_fixture.h"

#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {

void ContextFixture::SetUp(cl_uint numDevices, cl_device_id *pDeviceList) {
    auto retVal = CL_SUCCESS;
    pContext = Context::create<MockContext>(nullptr, ClDeviceVector(pDeviceList, numDevices),
                                            nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void ContextFixture::TearDown() {
    if (pContext != nullptr) {
        pContext->release();
    }
}
} // namespace NEO

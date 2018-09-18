/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "context_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

namespace OCLRT {

ContextFixture::ContextFixture()
    : pContext(nullptr) {
}

void ContextFixture::SetUp(cl_uint numDevices, cl_device_id *pDeviceList) {
    auto retVal = CL_SUCCESS;
    pContext = Context::create<MockContext>(nullptr, DeviceVector(pDeviceList, numDevices),
                                            nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void ContextFixture::TearDown() {
    delete pContext;
    pContext = nullptr;
}
} // namespace OCLRT

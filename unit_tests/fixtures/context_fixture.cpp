/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

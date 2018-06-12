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

#include "cl_api_tests.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/options.h"
#include "runtime/platform/platform.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {

api_fixture::api_fixture()
    : retVal(CL_SUCCESS),
      retSize(0), pContext(nullptr), pKernel(nullptr), pProgram(nullptr) {
}

void api_fixture::SetUp() {

    setReferenceTime();

    PlatformFixture::SetUp(numPlatformDevices, platformDevices);
    DeviceFixture::SetUp();
    ASSERT_NE(nullptr, pDevice);

    auto pDevice = pPlatform->getDevice(0);
    ASSERT_NE(nullptr, pDevice);

    cl_device_id clDevice = pDevice;
    pContext = Context::create(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    CommandQueueHwFixture::SetUp(pDevice, pContext);
}

void api_fixture::TearDown() {
    delete pKernel;
    delete pContext;
    delete pProgram;
    CommandQueueHwFixture::TearDown();
    DeviceFixture::TearDown();
    PlatformFixture::TearDown();
}
} // namespace OCLRT

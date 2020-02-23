/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/compiler_interface/compiler_interface.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/platform/platform.h"

#include "cl_api_tests.h"

namespace NEO {

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
} // namespace NEO

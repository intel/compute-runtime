/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClSVMFreeTests = ApiTests;

namespace ULT {

TEST_F(ClSVMFreeTests, GivenNullPtrWhenFreeingSvmThenNoAction) {
    clSVMFree(
        nullptr, // cl_context context
        nullptr  // void *svm_pointer
    );
}

TEST_F(ClSVMFreeTests, GivenContextWithDeviceNotSupportingSvmWhenFreeingSvmThenNoAction) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrSvm = false;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    cl_device_id deviceId = clDevice.get();
    auto context = clUniquePtr(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(retVal, CL_SUCCESS);

    clSVMFree(
        context.get(),
        reinterpret_cast<void *>(0x1234));
}

} // namespace ULT

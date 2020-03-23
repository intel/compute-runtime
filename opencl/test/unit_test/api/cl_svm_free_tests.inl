/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clSVMFreeTests;

namespace ULT {

TEST_F(clSVMFreeTests, GivenNullPtrWhenFreeingSvmThenNoAction) {
    clSVMFree(
        nullptr, // cl_context context
        nullptr  // void *svm_pointer
    );
}

TEST_F(clSVMFreeTests, GivenContextWithDeviceNotSupportingSvmWhenFreeingSvmThenNoAction) {
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

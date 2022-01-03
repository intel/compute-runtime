/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

namespace DeviceHostQueue {
struct deviceQueueProperties {
    static cl_queue_properties minimumProperties[5];
    static cl_queue_properties minimumPropertiesWithProfiling[5];
    static cl_queue_properties noProperties[5];
    static cl_queue_properties allProperties[5];
};

IGIL_CommandQueue getExpectedInitIgilCmdQueue(DeviceQueue *deviceQueue);
IGIL_CommandQueue getExpectedgilCmdQueueAfterReset(DeviceQueue *deviceQueue);

template <typename T>
class DeviceHostQueueFixture : public ApiFixture<>,
                               public ::testing::Test {
  public:
    void SetUp() override {
        ApiFixture::SetUp();
    }
    void TearDown() override {
        ApiFixture::TearDown();
    }

    cl_command_queue createClQueue(cl_queue_properties properties[5] = deviceQueueProperties::noProperties) {
        return create(pContext, testedClDevice, retVal, properties);
    }

    T *createQueueObject(cl_queue_properties properties[5] = deviceQueueProperties::noProperties) {
        using BaseType = typename T::BaseType;
        cl_context context = (cl_context)(pContext);
        auto clQueue = create(context, testedClDevice, retVal, properties);
        return castToObject<T>(static_cast<BaseType *>(clQueue));
    }

    cl_command_queue create(cl_context ctx, cl_device_id device, cl_int &retVal,
                            cl_queue_properties properties[5] = deviceQueueProperties::noProperties);
};

} // namespace DeviceHostQueue

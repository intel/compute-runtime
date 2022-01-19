/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"

using namespace NEO;

namespace DeviceHostQueue {
cl_queue_properties deviceQueueProperties::minimumProperties[5] = {
    CL_QUEUE_PROPERTIES,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    0, 0, 0};

cl_queue_properties deviceQueueProperties::minimumPropertiesWithProfiling[5] = {
    CL_QUEUE_PROPERTIES,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE,
    0, 0, 0};

cl_queue_properties deviceQueueProperties::noProperties[5] = {0};

cl_queue_properties deviceQueueProperties::allProperties[5] = {
    CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE,
    CL_QUEUE_SIZE, 128 * 1024,
    0};

template <>
cl_command_queue DeviceHostQueueFixture<CommandQueue>::create(cl_context ctx, cl_device_id device, cl_int &retVal,
                                                              cl_queue_properties properties[5]) {
    return clCreateCommandQueueWithProperties(ctx, device, properties, &retVal);
}
} // namespace DeviceHostQueue

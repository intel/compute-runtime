/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "opencl/source/command_stream/create_command_stream_impl.h"

namespace NEO {

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}

} // namespace NEO

/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/os_interface/device_factory.h"

namespace NEO {

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnviornment) {
    return getDevicesImpl(numDevicesReturned, executionEnviornment);
}

} // namespace NEO

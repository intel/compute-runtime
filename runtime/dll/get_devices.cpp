/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/device_factory.h"
#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/execution_environment/execution_environment.h"

namespace NEO {

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}

} // namespace NEO

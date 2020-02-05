/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "core/os_interface/device_factory.h"
#include "runtime/command_stream/create_command_stream_impl.h"

namespace NEO {

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}

} // namespace NEO

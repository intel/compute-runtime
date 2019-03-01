/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/device_factory.h"

namespace OCLRT {

bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnviornment) {
    return getDevicesImpl(hwInfo, numDevicesReturned, executionEnviornment);
}

} // namespace OCLRT

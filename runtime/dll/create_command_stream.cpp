/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/hw_info.h"

namespace OCLRT {

CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment) {
    return createCommandStreamImpl(pHwInfo, executionEnvironment);
}

bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnviornment) {
    return getDevicesImpl(hwInfo, numDevicesReturned, executionEnviornment);
}

} // namespace OCLRT

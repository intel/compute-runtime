/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/helpers/hw_info.h"

namespace OCLRT {

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment) {
    return createCommandStreamImpl(executionEnvironment);
}

} // namespace OCLRT

/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"

#include "command_stream/aub_command_stream_receiver.h"
#include "command_stream/command_stream_receiver_with_aub_dump.h"
#include "command_stream/create_command_stream_impl.h"
#include "command_stream/device_command_stream.h"
#include "command_stream/tbx_command_stream_receiver.h"

namespace NEO {

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    return createCommandStreamImpl(executionEnvironment, rootDeviceIndex);
}

} // namespace NEO

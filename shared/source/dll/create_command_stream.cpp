/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/create_command_stream_impl.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"

namespace NEO {

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                           uint32_t rootDeviceIndex,
                                           const DeviceBitfield deviceBitfield) {
    return createCommandStreamImpl(executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

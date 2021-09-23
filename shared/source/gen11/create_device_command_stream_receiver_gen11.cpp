/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"

#include "create_command_stream_receiver.inl"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<ICLFamily>(bool withAubDump,
                                                                    ExecutionEnvironment &executionEnvironment,
                                                                    uint32_t rootDeviceIndex,
                                                                    const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<ICLFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

#include "create_command_stream_receiver.inl"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<Xe3CoreFamily>(bool withAubDump,
                                                                        ExecutionEnvironment &executionEnvironment,
                                                                        uint32_t rootDeviceIndex,
                                                                        const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<Xe3CoreFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

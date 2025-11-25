/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/execution_environment/execution_environment.h"

#include "create_command_stream_receiver.inl"
#include "hw_cmds_xe3p_core.h"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<Xe3pCoreFamily>(bool withAubDump,
                                                                         ExecutionEnvironment &executionEnvironment,
                                                                         uint32_t rootDeviceIndex,
                                                                         const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<Xe3pCoreFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

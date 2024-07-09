/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

#include "create_command_stream_receiver.inl"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<Xe2HpgCoreFamily>(bool withAubDump,
                                                                           ExecutionEnvironment &executionEnvironment,
                                                                           uint32_t rootDeviceIndex,
                                                                           const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<Xe2HpgCoreFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

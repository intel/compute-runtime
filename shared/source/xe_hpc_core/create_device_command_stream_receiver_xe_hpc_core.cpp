/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "create_command_stream_receiver.inl"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<XeHpcCoreFamily>(bool withAubDump,
                                                                          ExecutionEnvironment &executionEnvironment,
                                                                          uint32_t rootDeviceIndex,
                                                                          const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<XeHpcCoreFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

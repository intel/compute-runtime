/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

#include "create_command_stream_receiver.inl"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<XeHpFamily>(bool withAubDump,
                                                                     ExecutionEnvironment &executionEnvironment,
                                                                     uint32_t rootDeviceIndex,
                                                                     const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<XeHpFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "create_command_stream_receiver.inl"

namespace NEO {

template <>
CommandStreamReceiver *createDeviceCommandStreamReceiver<XE_HPG_COREFamily>(bool withAubDump,
                                                                            ExecutionEnvironment &executionEnvironment,
                                                                            uint32_t rootDeviceIndex,
                                                                            const DeviceBitfield deviceBitfield) {
    return createCommandStreamReceiver<XE_HPG_COREFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}

} // namespace NEO

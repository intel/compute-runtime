/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_command_stream.inl"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiver *createCommandStreamReceiver(bool withAubDump,
                                                   ExecutionEnvironment &executionEnvironment,
                                                   uint32_t rootDeviceIndex,
                                                   const DeviceBitfield deviceBitfield) {
    return createDrmCommandStreamReceiver<GfxFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
}
} // namespace NEO

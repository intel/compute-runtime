/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Need to suppress warning 4005 caused by hw_cmds.h and wddm.h order.
// Current order must be preserved due to two versions of igfxfmid.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/windows/wddm_device_command_stream.h"

#pragma warning(pop)

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiver *createWddmCommandStreamReceiver(bool withAubDump,
                                                       ExecutionEnvironment &executionEnvironment,
                                                       uint32_t rootDeviceIndex,
                                                       const DeviceBitfield deviceBitfield) {
    if (withAubDump) {
        return new CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<GfxFamily>>(ApiSpecificConfig::getName(),
                                                                                          executionEnvironment,
                                                                                          rootDeviceIndex,
                                                                                          deviceBitfield);
    } else {
        return new WddmCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }
}
} // namespace NEO

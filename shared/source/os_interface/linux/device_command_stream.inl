/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiver *createDrmCommandStreamReceiver(bool withAubDump,
                                                      ExecutionEnvironment &executionEnvironment,
                                                      uint32_t rootDeviceIndex,
                                                      const DeviceBitfield deviceBitfield) {
    if (withAubDump) {
        return new CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<GfxFamily>>(ApiSpecificConfig::getName(),
                                                                                         executionEnvironment,
                                                                                         rootDeviceIndex,
                                                                                         deviceBitfield);
    } else {
        return new DrmCommandStreamReceiver<GfxFamily>(executionEnvironment,
                                                       rootDeviceIndex,
                                                       deviceBitfield);
    }
}
} // namespace NEO

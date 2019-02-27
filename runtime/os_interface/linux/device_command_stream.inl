/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/os_interface/linux/drm_command_stream.h"

#include "drm_command_stream.h"
#include "hw_cmds.h"

namespace OCLRT {

template <typename GfxFamily>
CommandStreamReceiver *DeviceCommandStreamReceiver<GfxFamily>::create(bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    if (withAubDump) {
        return new CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<GfxFamily>>("aubfile", executionEnvironment);
    } else {
        return new DrmCommandStreamReceiver<GfxFamily>(executionEnvironment);
    }
};
} // namespace OCLRT

/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Need to suppress warining 4005 caused by hw_cmds.h and wddm.h order.
// Current order must be preserved due to two versions of igfxfmid.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include "hw_cmds.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"
#pragma warning(pop)

namespace OCLRT {

template <typename GfxFamily>
CommandStreamReceiver *DeviceCommandStreamReceiver<GfxFamily>::create(const HardwareInfo &hwInfo, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    if (withAubDump) {
        return new CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<GfxFamily>>(hwInfo, executionEnvironment);
    } else {
        return new WddmCommandStreamReceiver<GfxFamily>(hwInfo, executionEnvironment);
    }
}
} // namespace OCLRT

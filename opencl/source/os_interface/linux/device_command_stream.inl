/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "opencl/source/os_interface/linux/drm_command_stream.h"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiver *DeviceCommandStreamReceiver<GfxFamily>::create(bool withAubDump,
                                                                      ExecutionEnvironment &executionEnvironment,
                                                                      uint32_t rootDeviceIndex,
                                                                      const DeviceBitfield deviceBitfield) {
    if (withAubDump) {
        return new CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<GfxFamily>>("aubfile",
                                                                                         executionEnvironment,
                                                                                         rootDeviceIndex,
                                                                                         deviceBitfield);
    } else {
        auto gemMode = gemCloseWorkerMode::gemCloseWorkerActive;

        if (DebugManager.flags.EnableDirectSubmission.get() == 1) {
            gemMode = gemCloseWorkerMode::gemCloseWorkerInactive;
        }

        if (DebugManager.flags.EnableGemCloseWorker.get() != -1) {
            gemMode = DebugManager.flags.EnableGemCloseWorker.get() ? gemCloseWorkerMode::gemCloseWorkerActive : gemCloseWorkerMode::gemCloseWorkerInactive;
        }

        return new DrmCommandStreamReceiver<GfxFamily>(executionEnvironment,
                                                       rootDeviceIndex,
                                                       deviceBitfield,
                                                       gemMode);
    }
};
} // namespace NEO

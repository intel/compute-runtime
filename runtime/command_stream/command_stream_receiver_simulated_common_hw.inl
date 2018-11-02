/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/os_interface/debug_settings_manager.h"
namespace OCLRT {
template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO() {
    auto newOffset = static_cast<uint32_t>(DebugManager.flags.AubDumpAddMmioRegister.get());
    if (newOffset > 0) {
        auto value = static_cast<uint32_t>(DebugManager.flags.AubDumpAddMmioRegisterValue.get());
        stream->writeMMIO(newOffset, value);
    }
}
} // namespace OCLRT

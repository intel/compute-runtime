/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
inline void CommandStreamReceiverHw<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream) {
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<Family>::getDcFlushEnable(true, peekHwInfo());
    args.textureCacheInvalidationEnable = true;
    args.hdcPipelineFlush = true;

    NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, peekHwInfo(), isRcs());
}

} // namespace NEO

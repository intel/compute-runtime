/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <>
void EncodeWA<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream,
                                                            const HardwareInfo &hwInfo, bool isRcs) {
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<Family>::getDcFlushEnable(true, hwInfo);
    args.textureCacheInvalidationEnable = true;
    args.hdcPipelineFlush = true;

    NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, hwInfo, isRcs);
}

} // namespace NEO

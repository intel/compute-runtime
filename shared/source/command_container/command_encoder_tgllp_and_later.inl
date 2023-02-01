/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <>
void EncodeWA<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream,
                                                            const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlushRequired) {
    PipeControlArgs args;
    args.dcFlushEnable = dcFlushRequired;
    args.textureCacheInvalidationEnable = true;
    args.hdcPipelineFlush = true;

    NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, rootDeviceEnvironment, isRcs);
}

} // namespace NEO

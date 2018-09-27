/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include "runtime/commands/bxml_generator_glue.h"
#include "runtime/helpers/debug_helpers.h"
#include "hw_info.h"
#include "igfxfmid.h"

//forward declaration for parsing logic
struct BdwParse;
namespace OCLRT {

struct GEN8 {
#include "runtime/gen8/hw_cmds_generated.h"
#include "runtime/gen8/hw_cmds_generated_patched.h"
};
struct BDWFamily : public GEN8 {
    using PARSE = BdwParse;
    using GfxFamily = BDWFamily;
    using WALKER_TYPE = GPGPU_WALKER;
    static const GPGPU_WALKER cmdInitGpgpuWalker;
    static const INTERFACE_DESCRIPTOR_DATA cmdInitInterfaceDescriptorData;
    static const MEDIA_INTERFACE_DESCRIPTOR_LOAD cmdInitMediaInterfaceDescriptorLoad;
    static const MEDIA_STATE_FLUSH cmdInitMediaStateFlush;
    static const MI_BATCH_BUFFER_END cmdInitBatchBufferEnd;
    static const MI_BATCH_BUFFER_START cmdInitBatchBufferStart;
    static const PIPE_CONTROL cmdInitPipeControl;

    static constexpr bool supportsCmdSet(GFXCORE_FAMILY cmdSetBaseFamily) {
        return cmdSetBaseFamily == IGFX_GEN8_CORE;
    }
};

} // namespace OCLRT

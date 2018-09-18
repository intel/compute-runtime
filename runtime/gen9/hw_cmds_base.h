/*
 * Copyright (C) 2017-2018 Intel Corporation
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

struct SklParse;

namespace OCLRT {

template <class GfxFamily>
class BaseInterfaceVersion;

struct GEN9 {
#include "runtime/gen9/hw_cmds_generated_patched.h"
#include "runtime/gen9/hw_cmds_generated.h"
};

struct SKLFamily : public GEN9 {
    typedef SklParse PARSE;
    typedef SKLFamily GfxFamily;
    typedef GPGPU_WALKER WALKER_TYPE;
    using HARDWARE_INTERFACE = BaseInterfaceVersion<SKLFamily>;
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

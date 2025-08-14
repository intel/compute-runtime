/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[IGFX_MAX_CORE];

using Family = XeHpcCoreFamily;
static auto gfxFamily = IGFX_XE_HPC_CORE;

template <typename GfxFamily>
struct GmmCallbacks;

struct EnableCoreXeHpcCore {
    EnableCoreXeHpcCore() {
        gfxCoreHelperFactory[gfxFamily] = GfxCoreHelperHw<Family>::create;
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreXeHpcCore enable;
} // namespace NEO

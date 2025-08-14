/*
 * Copyright (C) 2025 Intel Corporation
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
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[IGFX_MAX_CORE];

using Family = Xe3CoreFamily;
static auto gfxFamily = IGFX_XE3_CORE;

template <typename GfxFamily>
struct GmmCallbacks;

struct EnableCoreXe3Core {
    EnableCoreXe3Core() {
        gfxCoreHelperFactory[gfxFamily] = GfxCoreHelperHw<Family>::create;
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreXe3Core enable;
} // namespace NEO

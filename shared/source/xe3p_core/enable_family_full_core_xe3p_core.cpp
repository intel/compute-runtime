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

#include "hw_cmds_xe3p_core.h"

namespace NEO {

extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[NEO::maxCoreEnumValue];

using Family = Xe3pCoreFamily;
static auto gfxFamily = IGFX_XE3P_CORE;

template <typename GfxFamily>
struct GmmCallbacks;

struct EnableCoreXe3pHpgCore {
    EnableCoreXe3pHpgCore() {
        gfxCoreHelperFactory[gfxFamily] = GfxCoreHelperHw<Family>::create;
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreXe3pHpgCore enable;
} // namespace NEO

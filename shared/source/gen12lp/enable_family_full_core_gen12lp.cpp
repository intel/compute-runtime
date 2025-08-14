/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {

extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[IGFX_MAX_CORE];

using Family = Gen12LpFamily;
static auto gfxFamily = IGFX_GEN12LP_CORE;

template <typename GfxFamily>
struct GmmCallbacks;

struct EnableCoreGen12LP {
    EnableCoreGen12LP() {
        gfxCoreHelperFactory[gfxFamily] = GfxCoreHelperHw<Family>::create;
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreGen12LP enable;
} // namespace NEO

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

using Family = ICLFamily;
static auto gfxFamily = IGFX_GEN11_CORE;

struct EnableCoreGen11 {
    EnableCoreGen11() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
    }
};

static EnableCoreGen11 enable;
} // namespace NEO

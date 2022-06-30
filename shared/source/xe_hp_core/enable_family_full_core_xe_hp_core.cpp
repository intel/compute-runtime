/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

using Family = XeHpFamily;
static auto gfxFamily = IGFX_XE_HP_CORE;

struct EnableCoreXeHpCore {
    EnableCoreXeHpCore() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
    }
};

static EnableCoreXeHpCore enable;
} // namespace NEO

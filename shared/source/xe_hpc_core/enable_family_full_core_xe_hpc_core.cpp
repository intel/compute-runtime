/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

using Family = XE_HPC_COREFamily;
static auto gfxFamily = IGFX_XE_HPC_CORE;

struct EnableCoreXeHpcCore {
    EnableCoreXeHpcCore() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
    }
};

static EnableCoreXeHpcCore enable;
} // namespace NEO

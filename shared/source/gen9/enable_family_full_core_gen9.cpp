/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/populate_factory.h"

#include <type_traits>

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

using Family = SKLFamily;
static const auto gfxFamily = IGFX_GEN9_CORE;

struct EnableCoreGen9 {
    EnableCoreGen9() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
    }
};

static EnableCoreGen9 enable;
} // namespace NEO

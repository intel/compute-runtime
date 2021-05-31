/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/mem_obj/buffer.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace NEO {

typedef SKLFamily Family;

struct EnableL0Gen9 {
    EnableL0Gen9() {
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<BufferHw<Family>>();
        L0::populateFactoryTable<L0::L0HwHelperHw<Family>>();
    }
};

static EnableL0Gen9 enable;
} // namespace NEO

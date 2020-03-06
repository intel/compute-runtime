/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"

#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

typedef BDWFamily Family;

struct EnableL0Gen8 {
    EnableL0Gen8() {
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<BufferHw<Family>>();
    }
};

static EnableL0Gen8 enable;
} // namespace NEO

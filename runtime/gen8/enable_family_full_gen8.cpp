/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sampler/sampler.h"

#include "hw_cmds.h"

#include <type_traits>

namespace OCLRT {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef BDWFamily Family;
static const auto gfxFamily = IGFX_GEN8_CORE;

struct EnableGen8 {
    EnableGen8() {
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandQueueHw<Family>>();
        populateFactoryTable<DeviceQueueHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<BufferHw<Family>>();
        populateFactoryTable<ImageHw<Family>>();
        populateFactoryTable<SamplerHw<Family>>();
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableGen8 enable;
} // namespace OCLRT

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
#include "runtime/gen8/hw_cmds.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sampler/sampler.h"

#include <type_traits>

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef SKLFamily Family;
static const auto gfxFamily = IGFX_GEN9_CORE;

struct EnableGen9 {
    EnableGen9() {
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

static EnableGen9 enable;
} // namespace NEO

/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/gen12lp/hw_cmds.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sampler/sampler.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef TGLLPFamily Family;
static auto gfxFamily = IGFX_GEN12LP_CORE;

struct EnableGen12LP {
    EnableGen12LP() {
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

static EnableGen12LP enable;
} // namespace NEO

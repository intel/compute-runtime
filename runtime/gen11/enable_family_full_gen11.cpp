/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen11/hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/device_queue/device_queue_hw.h"

#ifdef HAVE_INSTRUMENTATION
#include "runtime/event/perf_counter.h"
#endif
#include "core/helpers/hw_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sampler/sampler.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef ICLFamily Family;
static auto gfxFamily = IGFX_GEN11_CORE;

struct EnableGen11 {
    EnableGen11() {
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

static EnableGen11 enable;
} // namespace NEO

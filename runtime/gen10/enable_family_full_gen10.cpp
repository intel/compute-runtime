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

#include "hw_cmds.h"
#ifdef HAVE_INSTRUMENTATION
#include "runtime/event/perf_counter.h"
#endif
#include "runtime/helpers/hw_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sampler/sampler.h"

namespace OCLRT {

#ifdef HAVE_INSTRUMENTATION
static_assert(std::is_same<SInstrQueryPerfCountersLayout, SInstrQueryPerfCountersLayout_Gen10>::value, "SInstrQueryPerfCountersLayout_Gen10 mismatch");
#endif

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef CNLFamily Family;
static auto gfxFamily = IGFX_GEN10_CORE;

struct EnableGen10 {
    EnableGen10() {
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

static EnableGen10 enable;
} // namespace OCLRT

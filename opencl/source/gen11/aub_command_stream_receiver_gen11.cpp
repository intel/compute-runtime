/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/command_stream/aub_command_stream_receiver_hw_bdw_plus.inl"
#include "opencl/source/helpers/base_object.h"

namespace NEO {

typedef ICLFamily Family;
static auto gfxCore = IGFX_GEN11_CORE;

template <>
constexpr uint32_t AUBCommandStreamReceiverHw<Family>::getMaskAndValueForPollForCompletion() {
    return 0x00008000;
}

template <>
void populateFactoryTable<AUBCommandStreamReceiverHw<Family>>() {
    extern AubCommandStreamReceiverCreateFunc aubCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, aubCommandStreamReceiverFactory));
    aubCommandStreamReceiverFactory[gfxCore] = AUBCommandStreamReceiverHw<Family>::create;
}

template class AUBCommandStreamReceiverHw<Family>;
} // namespace NEO

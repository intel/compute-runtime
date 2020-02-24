/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/array_count.h"

#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "opencl/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "opencl/source/command_stream/tbx_command_stream_receiver_hw.inl"
#include "opencl/source/helpers/base_object.h"

namespace NEO {
typedef ICLFamily Family;
static auto gfxCore = IGFX_GEN11_CORE;

template <>
uint32_t TbxCommandStreamReceiverHw<Family>::getMaskAndValueForPollForCompletion() const {
    return 0x80;
}

template <>
bool TbxCommandStreamReceiverHw<Family>::getpollNotEqualValueForPollForCompletion() const {
    return true;
}

template <>
void populateFactoryTable<TbxCommandStreamReceiverHw<Family>>() {
    extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, tbxCommandStreamReceiverFactory));
    tbxCommandStreamReceiverFactory[gfxCore] = TbxCommandStreamReceiverHw<Family>::create;
}

template class TbxCommandStreamReceiverHw<Family>;
template class CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<Family>>;
} // namespace NEO

/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.inl"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/array_count.h"

namespace OCLRT {
typedef CNLFamily Family;
static auto gfxCore = IGFX_GEN10_CORE;

template <>
void populateFactoryTable<TbxCommandStreamReceiverHw<Family>>() {
    extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, tbxCommandStreamReceiverFactory));
    tbxCommandStreamReceiverFactory[gfxCore] = TbxCommandStreamReceiverHw<Family>::create;
}

template class TbxCommandStreamReceiverHw<Family>;
template class CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<Family>>;
} // namespace OCLRT

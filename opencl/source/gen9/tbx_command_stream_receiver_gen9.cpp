/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"
#include "core/helpers/array_count.h"

#include "command_stream/command_stream_receiver_with_aub_dump.inl"
#include "command_stream/tbx_command_stream_receiver_hw.h"
#include "command_stream/tbx_command_stream_receiver_hw.inl"
#include "helpers/base_object.h"

namespace NEO {

typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
void populateFactoryTable<TbxCommandStreamReceiverHw<Family>>() {
    extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, tbxCommandStreamReceiverFactory));
    tbxCommandStreamReceiverFactory[gfxCore] = TbxCommandStreamReceiverHw<Family>::create;
}

template class TbxCommandStreamReceiverHw<Family>;
template class CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<Family>>;
} // namespace NEO

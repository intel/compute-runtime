/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.inl"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {
using Family = Xe3CoreFamily;
static auto gfxCore = IGFX_XE3_CORE;

template <>
void populateFactoryTable<TbxCommandStreamReceiverHw<Family>>() {
    extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, tbxCommandStreamReceiverFactory));
    tbxCommandStreamReceiverFactory[gfxCore] = TbxCommandStreamReceiverHw<Family>::create;
}

#include "shared/source/command_stream/tbx_command_stream_receiver_xehp_and_later.inl"

template class TbxCommandStreamReceiverHw<Family>;
template class CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<Family>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<Family>>>);
} // namespace NEO

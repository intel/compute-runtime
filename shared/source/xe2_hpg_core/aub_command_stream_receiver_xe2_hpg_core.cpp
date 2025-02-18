/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {
struct Xe2HpgCoreFamily;

using Family = Xe2HpgCoreFamily;
static auto gfxCore = IGFX_XE2_HPG_CORE;

template <>
void populateFactoryTable<AUBCommandStreamReceiverHw<Family>>() {
    extern AubCommandStreamReceiverCreateFunc aubCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, aubCommandStreamReceiverFactory));
    aubCommandStreamReceiverFactory[gfxCore] = AUBCommandStreamReceiverHw<Family>::create;
}

template class AUBCommandStreamReceiverHw<Family>;
static_assert(NEO::NonCopyableAndNonMovable<AUBCommandStreamReceiverHw<Family>>);
} // namespace NEO

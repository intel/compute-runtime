/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_xehp_and_later.inl"

namespace NEO {
typedef XeHpgCoreFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_xehp_and_later.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {
using Family = Xe2HpgCoreFamily;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

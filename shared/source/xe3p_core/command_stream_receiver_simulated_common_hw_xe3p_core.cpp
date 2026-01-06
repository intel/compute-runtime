/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_xehp_and_later.inl"
#include "shared/source/xe3p_core/hw_cmds_base.h"

namespace NEO {
using Family = Xe3pCoreFamily;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

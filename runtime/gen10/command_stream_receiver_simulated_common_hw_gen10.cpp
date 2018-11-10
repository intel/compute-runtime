/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.inl"

namespace OCLRT {
typedef CNLFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace OCLRT

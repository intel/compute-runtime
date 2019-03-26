/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.inl"

namespace NEO {
typedef CNLFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

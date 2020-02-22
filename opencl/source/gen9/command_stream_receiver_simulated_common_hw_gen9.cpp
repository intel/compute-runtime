/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw_bdw_plus.inl"

namespace NEO {
typedef SKLFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

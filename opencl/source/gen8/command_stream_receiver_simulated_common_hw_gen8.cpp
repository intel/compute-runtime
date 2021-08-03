/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw_bdw_and_later.inl"

namespace NEO {
typedef BDWFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

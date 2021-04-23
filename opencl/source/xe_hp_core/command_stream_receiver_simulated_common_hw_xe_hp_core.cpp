/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw_xehp_plus.inl"

namespace NEO {
typedef XeHpFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

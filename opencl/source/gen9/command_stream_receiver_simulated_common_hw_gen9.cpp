/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_bdw_and_later.inl"

namespace NEO {
typedef SKLFamily Family;

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "command_container/command_encoder.h"
#include "command_container/command_encoder.inl"
#include "command_container/command_encoder_base.inl"
#include "gen9/hw_cmds_base.h"
#include "opencl/source/gen9/reg_configs.h"

namespace NEO {

using Family = SKLFamily;

template struct EncodeDispatchKernel<Family>;
template struct EncodeStates<Family>;
template struct EncodeMathMMIO<Family>;
template struct EncodeIndirectParams<Family>;
template struct EncodeSetMMIO<Family>;
template struct EncodeL3State<Family>;
template struct EncodeMediaInterfaceDescriptorLoad<Family>;
template struct EncodeStateBaseAddress<Family>;
template struct EncodeStoreMMIO<Family>;
template struct EncodeSurfaceState<Family>;
template struct EncodeAtomic<Family>;
template struct EncodeSempahore<Family>;
template struct EncodeBatchBufferStartOrEnd<Family>;
template struct EncodeMiFlushDW<Family>;
} // namespace NEO

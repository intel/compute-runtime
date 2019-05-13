/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker_bdw_plus.inl"
#include "runtime/command_queue/hardware_interface_bdw_plus.inl"
#include "runtime/gen10/hw_info.h"

namespace NEO {

template class HardwareInterface<CNLFamily>;

template class GpgpuWalkerHelper<CNLFamily>;

template struct EnqueueOperation<CNLFamily>;

} // namespace NEO

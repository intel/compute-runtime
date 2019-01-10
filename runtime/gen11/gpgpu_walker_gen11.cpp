/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/gpgpu_walker.inl"
#include "runtime/command_queue/gpgpu_walker_base.inl"
#include "runtime/command_queue/hardware_interface.h"
#include "runtime/command_queue/hardware_interface.inl"
#include "runtime/command_queue/hardware_interface_base.inl"
#include "runtime/gen11/hw_info.h"

namespace NEO {

template class HardwareInterface<ICLFamily>;

template class GpgpuWalkerHelper<ICLFamily>;

template struct EnqueueOperation<ICLFamily>;

} // namespace NEO

/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_info.h"
#include "opencl/source/command_queue/gpgpu_walker_bdw_plus.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_plus.inl"

namespace NEO {

template class HardwareInterface<ICLFamily>;

template class GpgpuWalkerHelper<ICLFamily>;

template struct EnqueueOperation<ICLFamily>;

} // namespace NEO

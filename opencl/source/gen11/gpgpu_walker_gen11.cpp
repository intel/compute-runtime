/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen11/hw_info.h"

#include "command_queue/gpgpu_walker_bdw_plus.inl"
#include "command_queue/hardware_interface_bdw_plus.inl"

namespace NEO {

template class HardwareInterface<ICLFamily>;

template class GpgpuWalkerHelper<ICLFamily>;

template struct EnqueueOperation<ICLFamily>;

} // namespace NEO

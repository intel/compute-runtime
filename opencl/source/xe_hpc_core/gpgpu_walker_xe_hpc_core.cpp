/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "opencl/source/command_queue/gpgpu_walker_xehp_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_xehp_and_later.inl"

namespace NEO {

template class GpgpuWalkerHelper<XeHpcCoreFamily>;

template class HardwareInterface<XeHpcCoreFamily>;

template struct EnqueueOperation<XeHpcCoreFamily>;

} // namespace NEO

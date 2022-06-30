/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds.h"

#include "opencl/source/command_queue/gpgpu_walker_xehp_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_xehp_and_later.inl"

namespace NEO {

template class GpgpuWalkerHelper<XE_HPG_COREFamily>;

template class HardwareInterface<XE_HPG_COREFamily>;

template struct EnqueueOperation<XE_HPG_COREFamily>;

} // namespace NEO

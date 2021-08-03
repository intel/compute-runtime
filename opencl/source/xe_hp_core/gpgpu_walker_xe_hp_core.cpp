/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/gpgpu_walker_xehp_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_xehp_and_later.inl"

namespace NEO {

template class GpgpuWalkerHelper<XeHpFamily>;

template class HardwareInterface<XeHpFamily>;

template struct EnqueueOperation<XeHpFamily>;

} // namespace NEO

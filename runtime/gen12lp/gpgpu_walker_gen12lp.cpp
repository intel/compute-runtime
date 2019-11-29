/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_info.h"
#include "runtime/command_queue/gpgpu_walker_bdw_plus.inl"
#include "runtime/command_queue/hardware_interface_bdw_plus.inl"

namespace NEO {

template class HardwareInterface<TGLLPFamily>;

template <>
void GpgpuWalkerHelper<TGLLPFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<TGLLPFamily> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template class GpgpuWalkerHelper<TGLLPFamily>;

template struct EnqueueOperation<TGLLPFamily>;

} // namespace NEO

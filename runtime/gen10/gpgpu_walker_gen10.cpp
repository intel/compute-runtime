/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen10/hw_info.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/gpgpu_walker.inl"

namespace OCLRT {

template class GpgpuWalkerHelper<CNLFamily>;

template struct EnqueueOperation<CNLFamily>;

} // namespace OCLRT

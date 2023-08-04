/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger.h"

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"

namespace NEO {
void *Debugger::getDebugSurfaceReservedSurfaceState(IndirectHeap &ssh) {
    return ssh.getCpuBase();
}
} // namespace NEO
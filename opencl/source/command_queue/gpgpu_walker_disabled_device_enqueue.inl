/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/gpgpu_walker.h"

namespace NEO {
template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(
    LinearStream &commandStream,
    DeviceQueueHw<GfxFamily> &devQueueHw,
    PreemptionMode preemptionMode,
    SchedulerKernel &scheduler,
    IndirectHeap *ssh,
    IndirectHeap *dsh,
    bool isCcsUsed) {
    UNRECOVERABLE_IF(true);
}
} // namespace NEO
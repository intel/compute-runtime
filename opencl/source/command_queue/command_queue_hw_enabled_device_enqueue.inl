/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue_hw.h"

namespace NEO {

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::runSchedulerSimulation(DeviceQueueHw<GfxFamily> &devQueueHw, Kernel &parentKernel) {
    BuiltinKernelsSimulation::SchedulerSimulation<GfxFamily> simulation;
    simulation.runSchedulerSimulation(devQueueHw.getQueueBuffer(),
                                      devQueueHw.getStackBuffer(),
                                      devQueueHw.getEventPoolBuffer(),
                                      devQueueHw.getSlbBuffer(),
                                      devQueueHw.getDshBuffer(),
                                      parentKernel.getKernelReflectionSurface(),
                                      devQueueHw.getQueueStorageBuffer(),
                                      this->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getGraphicsAllocation(),
                                      devQueueHw.getDebugQueue());
}
} // namespace NEO

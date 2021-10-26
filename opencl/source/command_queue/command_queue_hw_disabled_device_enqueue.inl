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
}
} // namespace NEO

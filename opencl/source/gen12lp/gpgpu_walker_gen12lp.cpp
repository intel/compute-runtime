/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info.h"

#include "opencl/source/command_queue/gpgpu_walker_bdw_plus.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_plus.inl"

namespace NEO {

template <>
void GpgpuWalkerHelper<TGLLPFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<TGLLPFamily> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template <>
void GpgpuWalkerHelper<TGLLPFamily>::dispatchProfilingCommandsStart(
    TagNode<HwTimeStamps> &hwTimeStamps,
    LinearStream *commandStream,
    const HardwareInfo &hwInfo) {
    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<TGLLPFamily>::addPipeControlAndProgramPostSyncOperation(
        *commandStream,
        PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP,
        timeStampAddress,
        0llu,
        hwInfo,
        args);
}

template <>
void GpgpuWalkerHelper<TGLLPFamily>::dispatchProfilingCommandsEnd(
    TagNode<HwTimeStamps> &hwTimeStamps,
    LinearStream *commandStream,
    const HardwareInfo &hwInfo) {
    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, GlobalEndTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<TGLLPFamily>::addPipeControlAndProgramPostSyncOperation(
        *commandStream,
        PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP,
        timeStampAddress,
        0llu,
        hwInfo,
        args);
}

template class HardwareInterface<TGLLPFamily>;

template class GpgpuWalkerHelper<TGLLPFamily>;

template struct EnqueueOperation<TGLLPFamily>;

} // namespace NEO

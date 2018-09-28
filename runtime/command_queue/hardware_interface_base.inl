/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/hardware_interface.h"

namespace OCLRT {

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::getDefaultDshSpace(
    const size_t &offsetInterfaceDescriptorTable,
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    size_t &totalInterfaceDescriptorTableSize,
    Kernel *parentKernel,
    IndirectHeap *dsh,
    LinearStream *commandStream) {

    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    if (!parentKernel) {
        dsh->getSpace(totalInterfaceDescriptorTableSize);
    } else {
        dsh->getSpace(commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset() - dsh->getUsed());
    }
}

template <typename GfxFamily>
inline typename HardwareInterface<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *
HardwareInterface<GfxFamily>::obtainInterfaceDescriptorData(
    WALKER_TYPE<GfxFamily> *walkerCmd) {
    return nullptr;
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {

    if (enable) {
        PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(commandStream, commandQueue.getDevice());
        // Implement enabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
    } else {
        // Implement disabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
        PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(commandStream, commandQueue.getDevice());
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfStartCommands(
    const DispatchInfo &dispatchInfo,
    const MultiDispatchInfo &multiDispatchInfo,
    HwTimeStamps *hwTimeStamps,
    HwPerfCounter *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    if (&dispatchInfo == &*multiDispatchInfo.begin()) {
        // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
        if (hwTimeStamps != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream);
        }
        if (hwPerfCounter != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
        }
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfEndCommands(
    HwTimeStamps *hwTimeStamps,
    HwPerfCounter *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStamps is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(*hwTimeStamps, commandStream);
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
inline WALKER_TYPE<GfxFamily> *HardwareInterface<GfxFamily>::allocateWalkerSpace(LinearStream &commandStream,
                                                                                 const Kernel &kernel) {
    auto walkerCmd = static_cast<WALKER_TYPE<GfxFamily> *>(commandStream.getSpace(sizeof(WALKER_TYPE<GfxFamily>)));
    *walkerCmd = GfxFamily::cmdInitGpgpuWalker;
    return walkerCmd;
}

} // namespace OCLRT

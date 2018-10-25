/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_helper.h"
#include "runtime/scheduler/scheduler_kernel.h"

#include <cinttypes>

namespace OCLRT {

void SchedulerKernel::setArgs(GraphicsAllocation *queue,
                              GraphicsAllocation *commandsStack,
                              GraphicsAllocation *eventsPool,
                              GraphicsAllocation *secondaryBatchBuffer,
                              GraphicsAllocation *dsh,
                              GraphicsAllocation *reflectionSurface,
                              GraphicsAllocation *queueStorageBuffer,
                              GraphicsAllocation *ssh,
                              GraphicsAllocation *debugQueue) {

    setArgSvmAlloc(0, queue->getUnderlyingBuffer(), queue);
    setArgSvmAlloc(1, commandsStack->getUnderlyingBuffer(), commandsStack);
    setArgSvmAlloc(2, eventsPool->getUnderlyingBuffer(), eventsPool);
    setArgSvmAlloc(3, secondaryBatchBuffer->getUnderlyingBuffer(), secondaryBatchBuffer);
    setArgSvmAlloc(4, dsh->getUnderlyingBuffer(), dsh);
    setArgSvmAlloc(5, reflectionSurface->getUnderlyingBuffer(), reflectionSurface);
    setArgSvmAlloc(6, queueStorageBuffer->getUnderlyingBuffer(), queueStorageBuffer);
    setArgSvmAlloc(7, ssh->getUnderlyingBuffer(), ssh);
    if (debugQueue)
        setArgSvmAlloc(8, debugQueue->getUnderlyingBuffer(), debugQueue);

    DBG_LOG(PrintEMDebugInformation,
            "Scheduler Surfaces: \nqueue=", queue->getUnderlyingBuffer(), " \nstack=", commandsStack->getUnderlyingBuffer(),
            " \nevents=", eventsPool->getUnderlyingBuffer(), " \nslb=", secondaryBatchBuffer->getUnderlyingBuffer(), "\ndsh=", dsh->getUnderlyingBuffer(),
            " \nkrs=", reflectionSurface->getUnderlyingBuffer(), " \nstorage=", queueStorageBuffer->getUnderlyingBuffer(), "\nssh=", ssh->getUnderlyingBuffer());
}
void SchedulerKernel::computeGws() {
    auto &devInfo = device.getDeviceInfo();
    auto &hwInfo = device.getHardwareInfo();
    auto &helper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);

    size_t hWThreadsPerSubSlice = devInfo.maxComputUnits / hwInfo.pSysInfo->SubSliceCount;
    size_t wkgsPerSubSlice = hWThreadsPerSubSlice / PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20;

    wkgsPerSubSlice = std::min(wkgsPerSubSlice, helper.getMaxBarrierRegisterPerSlice());
    gws = wkgsPerSubSlice * hwInfo.pSysInfo->SubSliceCount * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 * PARALLEL_SCHEDULER_COMPILATION_SIZE_20;

    if (device.isSimulation()) {
        gws = PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 * PARALLEL_SCHEDULER_COMPILATION_SIZE_20;
    }
    if (DebugManager.flags.SchedulerGWS.get() != 0) {
        DEBUG_BREAK_IF(DebugManager.flags.SchedulerGWS.get() % (PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 * PARALLEL_SCHEDULER_COMPILATION_SIZE_20) != 0);
        gws = DebugManager.flags.SchedulerGWS.get();
    }

    DBG_LOG(PrintEMDebugInformation, "Scheduler GWS: ", gws);
    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "Scheduler GWS: %" PRIu64, static_cast<uint64_t>(gws));
}
} // namespace OCLRT

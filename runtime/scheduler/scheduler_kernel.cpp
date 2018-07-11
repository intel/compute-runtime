/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/hw_helper.h"
#include "runtime/scheduler/scheduler_kernel.h"

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
}
} // namespace OCLRT

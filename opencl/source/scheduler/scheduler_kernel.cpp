/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/scheduler/scheduler_kernel.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "opencl/source/device/cl_device.h"

#include <cinttypes>

namespace NEO {

void SchedulerKernel::setArgs(GraphicsAllocation *queue,
                              GraphicsAllocation *commandsStack,
                              GraphicsAllocation *eventsPool,
                              GraphicsAllocation *secondaryBatchBuffer,
                              GraphicsAllocation *dsh,
                              GraphicsAllocation *reflectionSurface,
                              GraphicsAllocation *queueStorageBuffer,
                              GraphicsAllocation *ssh,
                              GraphicsAllocation *debugQueue) {

    setArgSvmAlloc(0, reinterpret_cast<void *>(queue->getGpuAddress()), queue);
    setArgSvmAlloc(1, reinterpret_cast<void *>(commandsStack->getGpuAddress()), commandsStack);
    setArgSvmAlloc(2, reinterpret_cast<void *>(eventsPool->getGpuAddress()), eventsPool);
    setArgSvmAlloc(3, reinterpret_cast<void *>(secondaryBatchBuffer->getGpuAddress()), secondaryBatchBuffer);
    setArgSvmAlloc(4, reinterpret_cast<void *>(dsh->getGpuAddress()), dsh);
    setArgSvmAlloc(5, reinterpret_cast<void *>(reflectionSurface->getGpuAddress()), reflectionSurface);
    setArgSvmAlloc(6, reinterpret_cast<void *>(queueStorageBuffer->getGpuAddress()), queueStorageBuffer);
    setArgSvmAlloc(7, reinterpret_cast<void *>(ssh->getGpuAddress()), ssh);
    if (debugQueue)
        setArgSvmAlloc(8, reinterpret_cast<void *>(debugQueue->getGpuAddress()), debugQueue);

    DBG_LOG(PrintEMDebugInformation,
            "Scheduler Surfaces: \nqueue=", queue->getUnderlyingBuffer(), " \nstack=", commandsStack->getUnderlyingBuffer(),
            " \nevents=", eventsPool->getUnderlyingBuffer(), " \nslb=", secondaryBatchBuffer->getUnderlyingBuffer(), "\ndsh=", dsh->getUnderlyingBuffer(),
            " \nkrs=", reflectionSurface->getUnderlyingBuffer(), " \nstorage=", queueStorageBuffer->getUnderlyingBuffer(), "\nssh=", ssh->getUnderlyingBuffer());
}
void SchedulerKernel::computeGws() {
    auto &devInfo = device.getDeviceInfo();
    auto &hwInfo = device.getHardwareInfo();
    auto &helper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    size_t hWThreadsPerSubSlice = devInfo.maxComputUnits / hwInfo.gtSystemInfo.SubSliceCount;
    size_t wkgsPerSubSlice = hWThreadsPerSubSlice / PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20;

    wkgsPerSubSlice = std::min(wkgsPerSubSlice, helper.getMaxBarrierRegisterPerSlice());
    gws = wkgsPerSubSlice * hwInfo.gtSystemInfo.SubSliceCount * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 * PARALLEL_SCHEDULER_COMPILATION_SIZE_20;

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
BuiltinCode SchedulerKernel::loadSchedulerKernel(Device *device) {
    std::string schedulerResourceName = getFamilyNameWithType(device->getHardwareInfo()) + "_0_scheduler.builtin_kernel.bin";

    BuiltinCode ret;
    auto storage = std::make_unique<EmbeddedStorage>("");
    ret.resource = storage.get()->load(schedulerResourceName);
    ret.type = BuiltinCode::ECodeType::Binary;
    ret.targetDevice = device;
    return ret;
}
} // namespace NEO

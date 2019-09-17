/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/graphics_allocation.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"

#include <cstdint>
#include <mutex>
#include <thread>

using namespace std;
using namespace NEO;

namespace BuiltinKernelsSimulation {

template <typename GfxFamily>
void SchedulerSimulation<GfxFamily>::cleanSchedulerSimulation() {
    threadIDToLocalIDmap.clear();
    delete pGlobalBarrier;
}

template <typename GfxFamily>
void SchedulerSimulation<GfxFamily>::initializeSchedulerSimulation(GraphicsAllocation *queue,
                                                                   GraphicsAllocation *commandsStack,
                                                                   GraphicsAllocation *eventsPool,
                                                                   GraphicsAllocation *secondaryBatchBuffer,
                                                                   GraphicsAllocation *dsh,
                                                                   GraphicsAllocation *reflectionSurface,
                                                                   GraphicsAllocation *queueStorageBuffer,
                                                                   GraphicsAllocation *ssh,
                                                                   GraphicsAllocation *debugQueue) {

    localSize[0] = NUM_OF_THREADS;
    localSize[1] = 1;
    localSize[2] = 1;

    threadIDToLocalIDmap.clear();
    pGlobalBarrier = new SynchronizationBarrier(NUM_OF_THREADS);

    // Spawn Thread ID == 0 on main thread
    for (uint32_t i = 1; i < NUM_OF_THREADS; i++) {
        threads[i] = std::thread(startScheduler, i, queue, commandsStack, eventsPool, secondaryBatchBuffer, dsh, reflectionSurface, queueStorageBuffer, ssh, debugQueue);
    }

    conditionReady = true;
}

template <typename GfxFamily>
void SchedulerSimulation<GfxFamily>::runSchedulerSimulation(GraphicsAllocation *queue,
                                                            GraphicsAllocation *commandsStack,
                                                            GraphicsAllocation *eventsPool,
                                                            GraphicsAllocation *secondaryBatchBuffer,
                                                            GraphicsAllocation *dsh,
                                                            GraphicsAllocation *reflectionSurface,
                                                            GraphicsAllocation *queueStorageBuffer,
                                                            GraphicsAllocation *ssh,
                                                            GraphicsAllocation *debugQueue) {
    simulationRun = true;
    if (enabled) {
        initializeSchedulerSimulation(queue,
                                      commandsStack,
                                      eventsPool,
                                      secondaryBatchBuffer,
                                      dsh,
                                      reflectionSurface,
                                      queueStorageBuffer,
                                      ssh,
                                      debugQueue);

        // start main thread with LID == 0
        startScheduler(0,
                       queue,
                       commandsStack,
                       eventsPool,
                       secondaryBatchBuffer,
                       dsh,
                       reflectionSurface,
                       queueStorageBuffer,
                       ssh,
                       debugQueue);

        // Wait for all threads on main thread
        if (threadIDToLocalIDmap[std::this_thread::get_id()] == 0) {

            for (uint32_t i = 1; i < NUM_OF_THREADS; i++)
                threads[i].join();

            cleanSchedulerSimulation();
        }
    }
};

} // namespace BuiltinKernelsSimulation

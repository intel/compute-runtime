/*
* Copyright (c) 2017, Intel Corporation
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

#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"

#include <cstdint>
#include <mutex>
#include <thread>

using namespace std;
using namespace OCLRT;

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

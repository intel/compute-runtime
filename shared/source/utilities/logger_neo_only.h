/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/logger.h"

#include <sstream>

/* Should not be used outside of NEO (e.g. with ocloc) because of dependencies towards other NEO structures */
namespace NEO {

template <DebugFunctionalityLevel level>
void logAllocation(FileLogger<level> &logger, GraphicsAllocation const *graphicsAllocation, MemoryManager const *memoryManager) {
    if (logger.shouldLogAllocationType()) {
        printDebugString(true, stdout, "Created Graphics Allocation of type %s\n", getAllocationTypeString(graphicsAllocation));
    }

    if (!logger.enabled() && !logger.shouldLogAllocationToStdout()) {
        return;
    }

    if (logger.shouldLogAllocationMemoryPool() || logger.shouldLogAllocationType()) {
        std::stringstream ss;
        std::thread::id thisThread = std::this_thread::get_id();

        ss << " ThreadID: " << thisThread;
        ss << " Type: " << getAllocationTypeString(graphicsAllocation);
        ss << " Pool: " << getMemoryPoolString(graphicsAllocation);
        ss << " Root index: " << graphicsAllocation->getRootDeviceIndex();
        ss << " Size: " << graphicsAllocation->getUnderlyingBufferSize();
        ss << " GPU VA: 0x" << std::hex << graphicsAllocation->getGpuAddress() << " - 0x" << std::hex << graphicsAllocation->getGpuAddress() + graphicsAllocation->getUnderlyingBufferSize() - 1;

        ss << graphicsAllocation->getAllocationInfoString();

        if (memoryManager) {
            auto *rootDeviceEnvironment{[graphicsAllocation, memoryManager]() {
                auto &rootDeviceEnvironments{memoryManager->peekExecutionEnvironment().rootDeviceEnvironments};
                auto indexOfAllocation = graphicsAllocation->getRootDeviceIndex();
                return (indexOfAllocation < rootDeviceEnvironments.size()) ? rootDeviceEnvironments[indexOfAllocation].get() : nullptr;
            }()};
            if (rootDeviceEnvironment) {
                ss << graphicsAllocation->getPatIndexInfoString(rootDeviceEnvironment->getProductHelper());
            }

            ss << " Total sys mem allocated: " << std::dec << memoryManager->getUsedSystemMemorySize();
            ss << " Total lmem allocated: " << std::dec << memoryManager->getUsedLocalMemorySize(graphicsAllocation->getRootDeviceIndex());
        }

        ss << std::endl;
        auto str = ss.str();
        if (logger.shouldLogAllocationToStdout()) {
            printf("%s", str.c_str());
            return;
        }

        if (logger.enabled()) {
            logger.writeToFile(logger.getLogFileNameString(), str.c_str(), str.size(), std::ios::app);
        }
    }
}

} // namespace NEO

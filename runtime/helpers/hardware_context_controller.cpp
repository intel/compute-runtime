/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_context_controller.h"

#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/os_context.h"

using namespace OCLRT;

HardwareContextController::HardwareContextController(aub_stream::AubManager &aubManager, OsContext &osContext, uint32_t flags) {
    auto deviceBitfield = osContext.getDeviceBitfield();
    for (uint32_t deviceIndex = 0; deviceIndex < deviceBitfield.size(); deviceIndex++) {
        if (deviceBitfield.test(deviceIndex)) {
            hardwareContexts.emplace_back(aubManager.createHardwareContext(deviceIndex, osContext.getEngineType(), flags));
        }
    }
}

void HardwareContextController::initialize() {
    for (auto &hardwareContext : hardwareContexts) {
        hardwareContext->initialize();
    }
}

void HardwareContextController::pollForCompletion() {
    for (auto &hardwareContext : hardwareContexts) {
        hardwareContext->pollForCompletion();
    }
}

void HardwareContextController::expectMemory(uint64_t gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) {
    for (auto &hardwareContext : hardwareContexts) {
        hardwareContext->expectMemory(gfxAddress, srcAddress, length, compareOperation);
    }
}

void HardwareContextController::submit(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize,
                                       uint32_t memoryBank, uint64_t entryBits) {
    for (auto &hardwareContext : hardwareContexts) {
        hardwareContext->submit(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, MemoryConstants::pageSize64k);
    }
}

void HardwareContextController::writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize) {
    for (auto &hardwareContext : hardwareContexts) {
        hardwareContext->writeMemory(gfxAddress, memory, size, memoryBanks, hint, pageSize);
    }
}

void HardwareContextController::dumpBufferBIN(uint64_t gfxAddress, size_t size) {
    hardwareContexts[0]->dumpBufferBIN(gfxAddress, size);
}

void HardwareContextController::readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBanks, size_t pageSize) {
    hardwareContexts[0]->readMemory(gfxAddress, memory, size, memoryBanks, pageSize);
}

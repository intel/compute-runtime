/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hardware_context_controller.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/os_context.h"

#include "third_party/aub_stream/headers/allocation_params.h"
using namespace NEO;

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
                                       uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead) {
    for (auto &hardwareContext : hardwareContexts) {
        hardwareContext->submitBatchBuffer(batchBufferGpuAddress, overrideRingHead);
    }
}

void HardwareContextController::writeMemory(aub_stream::AllocationParams &allocationParams) {
    if (hardwareContexts.size() == 1u) {
        hardwareContexts.at(0)->writeMemory2(allocationParams);
        return;
    }

    aub_stream::AllocationParams clonedParams = allocationParams;

    for (auto bankId = 0u; bankId < hardwareContexts.size(); bankId++) {
        auto &hardwareContext = hardwareContexts.at(bankId);

        auto selectedBank = allocationParams.memoryBanks & (1 << bankId);
        UNRECOVERABLE_IF(selectedBank == 0);

        clonedParams.memoryBanks = selectedBank;
        hardwareContext->writeMemory2(clonedParams);
    }
}

void HardwareContextController::writeMMIO(uint32_t offset, uint32_t value) {
    hardwareContexts[0]->writeMMIO(offset, value);
}

void HardwareContextController::dumpBufferBIN(uint64_t gfxAddress, size_t size) {
    hardwareContexts[0]->dumpBufferBIN(gfxAddress, size);
}

void HardwareContextController::dumpSurface(const aub_stream::SurfaceInfo &surfaceInfo) {
    hardwareContexts[0]->dumpSurface(surfaceInfo);
}

void HardwareContextController::readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBanks, size_t pageSize) {
    hardwareContexts[0]->readMemory(gfxAddress, memory, size, memoryBanks, pageSize);
}

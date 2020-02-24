/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/block_kernel_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/debug_helpers.h"

#include "opencl/source/program/kernel_info.h"

namespace NEO {

void BlockKernelManager::addBlockKernelInfo(KernelInfo *blockKernelInfo) {
    blockKernelInfoArray.push_back(blockKernelInfo);
    blockUsesPrintf |= (blockKernelInfo->patchInfo.pAllocateStatelessPrintfSurface != nullptr);
}

const KernelInfo *BlockKernelManager::getBlockKernelInfo(size_t ordinal) {
    DEBUG_BREAK_IF(ordinal >= blockKernelInfoArray.size());
    return blockKernelInfoArray[ordinal];
}

BlockKernelManager::~BlockKernelManager() {
    for (auto &i : blockKernelInfoArray)
        delete i;
}
void BlockKernelManager::pushPrivateSurface(GraphicsAllocation *allocation, size_t ordinal) {
    if (blockPrivateSurfaceArray.size() < blockKernelInfoArray.size()) {
        blockPrivateSurfaceArray.resize(blockKernelInfoArray.size(), nullptr);
    }

    DEBUG_BREAK_IF(ordinal >= blockPrivateSurfaceArray.size());

    blockPrivateSurfaceArray[ordinal] = allocation;
}

GraphicsAllocation *BlockKernelManager::getPrivateSurface(size_t ordinal) {
    // Ff queried ordinal is out of bound return nullptr,
    // this happens when no private surface was not pushed
    if (ordinal < blockPrivateSurfaceArray.size())
        return blockPrivateSurfaceArray[ordinal];
    return nullptr;
}
void BlockKernelManager::makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver) {
    auto blockCount = blockKernelInfoArray.size();
    for (uint32_t surfaceIndex = 0; surfaceIndex < blockCount; surfaceIndex++) {
        auto surface = getPrivateSurface(surfaceIndex);
        if (surface) {
            commandStreamReceiver.makeResident(*surface);
        }
        surface = blockKernelInfoArray[surfaceIndex]->getGraphicsAllocation();
        if (surface) {
            commandStreamReceiver.makeResident(*surface);
        }
    }
}
} // namespace NEO

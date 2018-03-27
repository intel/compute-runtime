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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/program/block_kernel_manager.h"
#include "runtime/program/kernel_info.h"

namespace OCLRT {

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
        blockPrivateSurfaceArray.resize(blockKernelInfoArray.size());

        for (uint32_t i = 0; i < blockPrivateSurfaceArray.size(); i++) {
            blockPrivateSurfaceArray[i] = nullptr;
        }
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
} // namespace OCLRT
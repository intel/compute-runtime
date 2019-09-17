/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/string.h"
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

template <typename GfxFamily>
GraphicsAllocation *FlatBatchBufferHelperHw<GfxFamily>::flattenBatchBuffer(BatchBuffer &batchBuffer, size_t &sizeBatchBuffer,
                                                                           DispatchMode dispatchMode) {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename GfxFamily::MI_USER_INTERRUPT MI_USER_INTERRUPT;

    GraphicsAllocation *flatBatchBuffer = nullptr;

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;
    std::unique_ptr<char> indirectPatchCommands(getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));

    if (dispatchMode == DispatchMode::ImmediateDispatch) {
        if (batchBuffer.chainedBatchBuffer) {
            batchBuffer.chainedBatchBuffer->setAubWritable(false, GraphicsAllocation::defaultBank);
            auto sizeMainBatchBuffer = batchBuffer.chainedBatchBufferStartOffset - batchBuffer.startOffset;
            auto alignedMainBatchBufferSize = alignUp(sizeMainBatchBuffer + indirectPatchCommandsSize + batchBuffer.chainedBatchBuffer->getUnderlyingBufferSize(), MemoryConstants::pageSize);
            AllocationProperties flatBatchBufferProperties(alignedMainBatchBufferSize, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY);
            flatBatchBufferProperties.alignment = MemoryConstants::pageSize;
            flatBatchBuffer =
                getMemoryManager()->allocateGraphicsMemoryWithProperties(flatBatchBufferProperties);
            UNRECOVERABLE_IF(flatBatchBuffer == nullptr);
            // Copy main batchbuffer
            memcpy_s(flatBatchBuffer->getUnderlyingBuffer(), sizeMainBatchBuffer,
                     ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset),
                     sizeMainBatchBuffer);
            // Copy indirect patch commands
            memcpy_s(ptrOffset(flatBatchBuffer->getUnderlyingBuffer(), sizeMainBatchBuffer), indirectPatchCommandsSize,
                     indirectPatchCommands.get(), indirectPatchCommandsSize);
            // Copy chained batchbuffer
            memcpy_s(ptrOffset(flatBatchBuffer->getUnderlyingBuffer(), sizeMainBatchBuffer + indirectPatchCommandsSize),
                     batchBuffer.chainedBatchBuffer->getUnderlyingBufferSize(), batchBuffer.chainedBatchBuffer->getUnderlyingBuffer(),
                     batchBuffer.chainedBatchBuffer->getUnderlyingBufferSize());
            sizeBatchBuffer = flatBatchBufferProperties.size;
            patchInfoCollection.insert(std::end(patchInfoCollection), std::begin(indirectPatchInfo), std::end(indirectPatchInfo));
        }
    } else if (dispatchMode == DispatchMode::BatchedDispatch) {
        CommandChunk firstChunk;
        for (auto &chunk : commandChunkList) {
            bool found = false;
            for (auto &batchBuffer : batchBufferStartAddressSequence) {
                if ((batchBuffer.first <= chunk.baseAddressGpu + chunk.endOffset) && (batchBuffer.first >= chunk.baseAddressGpu + chunk.startOffset)) {
                    chunk.batchBufferStartLocation = batchBuffer.first;
                    chunk.batchBufferStartAddress = batchBuffer.second;
                    chunk.endOffset = chunk.batchBufferStartLocation - chunk.baseAddressGpu;
                }
                if (batchBuffer.second == chunk.baseAddressGpu + chunk.startOffset) {
                    found = true;
                }
            }
            if (!found) {
                firstChunk = chunk;
            }
        }

        std::vector<CommandChunk> orderedChunks;
        CommandChunk &nextChunk = firstChunk;
        while (true) {
            bool hasNextChunk = false;
            for (auto &chunk : commandChunkList) {
                if (nextChunk.batchBufferStartAddress == chunk.baseAddressGpu + chunk.startOffset) {
                    hasNextChunk = true;
                    orderedChunks.push_back(nextChunk);
                    nextChunk = chunk;
                    break;
                }
            }
            if (!hasNextChunk) {
                nextChunk.endOffset -= sizeof(MI_BATCH_BUFFER_START);
                orderedChunks.push_back(nextChunk);
                break;
            }
        }

        uint64_t flatBatchBufferSize = 0u;
        std::vector<PatchInfoData> patchInfoCopy = patchInfoCollection;
        patchInfoCollection.clear();

        for (auto &chunk : orderedChunks) {
            for (auto &patch : patchInfoCopy) {
                if (patch.targetAllocation + patch.targetAllocationOffset >= chunk.baseAddressGpu + chunk.startOffset && patch.targetAllocation + patch.targetAllocationOffset <= chunk.baseAddressGpu + chunk.endOffset) {
                    patch.targetAllocationOffset = patch.targetAllocationOffset - chunk.startOffset + flatBatchBufferSize + indirectPatchCommandsSize;
                    patchInfoCollection.push_back(patch);
                }
            }
            flatBatchBufferSize += chunk.endOffset - chunk.startOffset;
        }
        patchInfoCollection.insert(std::end(patchInfoCollection), std::begin(indirectPatchInfo), std::end(indirectPatchInfo));

        flatBatchBufferSize += sizeof(MI_USER_INTERRUPT);
        flatBatchBufferSize += sizeof(MI_BATCH_BUFFER_END);
        flatBatchBufferSize += indirectPatchCommandsSize;

        flatBatchBufferSize = alignUp(flatBatchBufferSize, MemoryConstants::pageSize);
        flatBatchBufferSize += CSRequirements::csOverfetchSize;
        AllocationProperties flatBatchBufferProperties(static_cast<size_t>(flatBatchBufferSize), GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY);
        flatBatchBufferProperties.alignment = MemoryConstants::pageSize;
        flatBatchBuffer = getMemoryManager()->allocateGraphicsMemoryWithProperties(flatBatchBufferProperties);
        UNRECOVERABLE_IF(flatBatchBuffer == nullptr);

        char *ptr = static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer());
        memcpy_s(ptr, indirectPatchCommandsSize, indirectPatchCommands.get(), indirectPatchCommandsSize);
        ptr += indirectPatchCommandsSize;
        for (auto &chunk : orderedChunks) {
            size_t chunkSize = static_cast<size_t>(chunk.endOffset - chunk.startOffset);
            memcpy_s(ptr,
                     chunkSize,
                     reinterpret_cast<char *>(ptrOffset(chunk.baseAddressCpu, static_cast<size_t>(chunk.startOffset))),
                     chunkSize);
            ptr += chunkSize;
        }

        auto pCmdMui = reinterpret_cast<MI_USER_INTERRUPT *>(ptr);
        *pCmdMui = GfxFamily::cmdInitUserInterrupt;
        ptr += sizeof(MI_USER_INTERRUPT);

        auto pCmdBBend = reinterpret_cast<MI_BATCH_BUFFER_END *>(ptr);
        *pCmdBBend = GfxFamily::cmdInitBatchBufferEnd;
        ptr += sizeof(MI_BATCH_BUFFER_END);

        sizeBatchBuffer = static_cast<size_t>(flatBatchBufferSize);
        commandChunkList.clear();
        batchBufferStartAddressSequence.clear();
    }

    return flatBatchBuffer;
}

template <typename GfxFamily>
char *FlatBatchBufferHelperHw<GfxFamily>::getIndirectPatchCommands(size_t &indirectPatchCommandsSize, std::vector<PatchInfoData> &indirectPatchInfo) {
    typedef typename GfxFamily::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;

    indirectPatchCommandsSize = 0;
    for (auto &patchInfoData : patchInfoCollection) {
        if (patchInfoData.requiresIndirectPatching()) {
            indirectPatchCommandsSize += sizeof(MI_STORE_DATA_IMM);
        }
    }

    uint64_t stiCommandOffset = 0;
    std::vector<PatchInfoData> patchInfoCopy = patchInfoCollection;
    std::unique_ptr<char> buffer(new char[indirectPatchCommandsSize]);
    LinearStream indirectPatchCommandStream(buffer.get(), indirectPatchCommandsSize);
    patchInfoCollection.clear();

    for (auto &patchInfoData : patchInfoCopy) {
        if (patchInfoData.requiresIndirectPatching()) {
            auto storeDataImmediate = indirectPatchCommandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
            *storeDataImmediate = GfxFamily::cmdInitStoreDataImm;
            storeDataImmediate->setAddress(patchInfoData.targetAllocation + patchInfoData.targetAllocationOffset);
            storeDataImmediate->setStoreQword(patchInfoData.patchAddressSize != sizeof(uint32_t));
            storeDataImmediate->setDataDword0(static_cast<uint32_t>((patchInfoData.sourceAllocation + patchInfoData.sourceAllocationOffset) & 0x0000FFFFFFFFULL));
            storeDataImmediate->setDataDword1(static_cast<uint32_t>((patchInfoData.sourceAllocation + patchInfoData.sourceAllocationOffset) >> 32));

            PatchInfoData patchInfoForAddress(patchInfoData.targetAllocation, patchInfoData.targetAllocationOffset, patchInfoData.targetType, 0u, stiCommandOffset + sizeof(MI_STORE_DATA_IMM) - 2 * sizeof(uint64_t), PatchInfoAllocationType::Default);
            PatchInfoData patchInfoForValue(patchInfoData.sourceAllocation, patchInfoData.sourceAllocationOffset, patchInfoData.sourceType, 0u, stiCommandOffset + sizeof(MI_STORE_DATA_IMM) - sizeof(uint64_t), PatchInfoAllocationType::Default);
            indirectPatchInfo.push_back(patchInfoForAddress);
            indirectPatchInfo.push_back(patchInfoForValue);
            stiCommandOffset += sizeof(MI_STORE_DATA_IMM);
        } else {
            patchInfoCollection.push_back(patchInfoData);
        }
    }
    return buffer.release();
}
template <typename GfxFamily>
void FlatBatchBufferHelperHw<GfxFamily>::removePipeControlData(size_t pipeControlLocationSize, void *pipeControlForNooping) {
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    size_t numPipeControls = pipeControlLocationSize / sizeof(PIPE_CONTROL);
    for (size_t i = 0; i < numPipeControls; i++) {
        PIPE_CONTROL *erasedPipeControl = reinterpret_cast<PIPE_CONTROL *>(pipeControlForNooping);
        removePatchInfoData(reinterpret_cast<uint64_t>(erasedPipeControl) + (i + 1) * sizeof(PIPE_CONTROL) - 2 * sizeof(uint64_t));
        removePatchInfoData(reinterpret_cast<uint64_t>(erasedPipeControl) + (i + 1) * sizeof(PIPE_CONTROL) - sizeof(uint64_t));
    }
}

template <typename GfxFamily>
void FlatBatchBufferHelperHw<GfxFamily>::collectScratchSpacePatchInfo(uint64_t scratchAddress, uint64_t commandOffset, const LinearStream &csr) {
    if (scratchAddress) {
        auto scratchOffset = reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(csr.getCpuBase()) + commandOffset)[0] & 0x3FF;
        PatchInfoData patchInfoData(scratchAddress, scratchOffset, PatchInfoAllocationType::ScratchSpace, csr.getGraphicsAllocation()->getGpuAddress(), commandOffset, PatchInfoAllocationType::Default);
        patchInfoCollection.push_back(patchInfoData);
    }
}

}; // namespace NEO

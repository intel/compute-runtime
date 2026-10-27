/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

using UploadedSections = std::vector<std::pair<uint64_t, size_t>>;

inline UploadedSections getUploadedSectionsOfAllocation(const MockAubManager &mockAubManager, const NEO::GraphicsAllocation &allocation) {
    UploadedSections uploadedSections;
    auto allocationStart = allocation.getGpuAddress();
    auto allocationEnd = allocationStart + allocation.getUnderlyingBufferSize();
    for (const auto &params : mockAubManager.storedAllocationParams) {
        if (params.gfxAddress >= allocationStart && params.gfxAddress < allocationEnd) {
            uploadedSections.emplace_back(params.gfxAddress - allocationStart, params.size);
        }
    }
    return uploadedSections;
}

struct DirectSubmissionCommandBufferUploadResult {
    bool usedCommandBufferRangeUploaded = false;
    bool returnPatchUploaded = false;
    bool tbxWritableAfterFlush = false;
    bool aubWritableAfterFlush = false;
};

template <typename FamilyType>
DirectSubmissionCommandBufferUploadResult flushCommandBufferOnStartedDirectSubmission(CommandStreamReceiver &csr, MockAubManager &mockAubManager, MemoryManager &memoryManager, const DeviceBitfield deviceBitfield) {
    auto commandBuffer = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr.getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::commandBuffer, deviceBitfield});
    commandBuffer->setSimulationInitialUploadDone(true);

    LinearStream commandStream(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(commandStream.getGraphicsAllocation(), &commandStream, commandStream.getUsed());
    batchBuffer.endCmdPtr = commandStream.getCpuBase();
    ResidencyContainer allocationsForResidency;
    csr.flush(batchBuffer, allocationsForResidency);

    constexpr size_t returnPatchOffset = 128u;
    constexpr size_t usedCommandBufferSize = 256u;
    commandStream.getSpace(usedCommandBufferSize);
    batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(commandStream.getGraphicsAllocation(), &commandStream, commandStream.getUsed());
    batchBuffer.endCmdPtr = ptrOffset(commandStream.getCpuBase(), returnPatchOffset);
    batchBuffer.disableFlatRingBuffer = true;
    mockAubManager.storedAllocationParams.clear();
    mockAubManager.storeAllocationParams = true;
    csr.flush(batchBuffer, allocationsForResidency);

    auto isRangeUploaded = [&mockAubManager](uint64_t gpuAddress, size_t size) {
        return std::any_of(mockAubManager.storedAllocationParams.begin(), mockAubManager.storedAllocationParams.end(), [gpuAddress, size](const aub_stream::AllocationParams &params) {
            return params.gfxAddress == gpuAddress && params.size == size;
        });
    };

    DirectSubmissionCommandBufferUploadResult result;
    result.usedCommandBufferRangeUploaded = isRangeUploaded(commandBuffer->getGpuAddress(), usedCommandBufferSize);
    result.returnPatchUploaded = isRangeUploaded(commandBuffer->getGpuAddress() + returnPatchOffset, sizeof(typename FamilyType::MI_BATCH_BUFFER_START));
    result.tbxWritableAfterFlush = commandBuffer->isTbxWritable(GraphicsAllocation::defaultBank);
    result.aubWritableAfterFlush = commandBuffer->isAubWritable(GraphicsAllocation::defaultBank);

    memoryManager.freeGraphicsMemory(commandBuffer);
    return result;
}

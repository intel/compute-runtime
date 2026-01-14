/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/host_function.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
void HostFunctionHelper<GfxFamily>::programHostFunction(LinearStream &commandStream, HostFunctionStreamer &streamer, HostFunction &&hostFunction, bool isMemorySynchronizationRequired) {
    HostFunctionHelper<GfxFamily>::programHostFunctionId(&commandStream, nullptr, streamer, std::move(hostFunction), isMemorySynchronizationRequired);

    auto nPartitions = streamer.getActivePartitions();
    for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
        HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(&commandStream, nullptr, streamer, partitionId);
    }
}

template <typename GfxFamily>
void HostFunctionHelper<GfxFamily>::programHostFunctionId(LinearStream *commandStream, void *cmdBuffer, HostFunctionStreamer &streamer, HostFunction &&hostFunction, bool isMemorySynchronizationRequired) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    auto partitionId = 0u; // get address of partition 0, other partitions will use workloadPartitionIdOffset
    auto idGpuAddress = streamer.getHostFunctionIdGpuAddress(partitionId);
    auto hostFunctionId = streamer.getNextHostFunctionIdAndIncrement();
    streamer.addHostFunction(hostFunctionId, std::move(hostFunction));

    bool workloadPartitionIdOffsetEnable = streamer.getActivePartitions() > 1;
    bool isDcFlushRequired = streamer.getDcFlushRequired();

    if (isMemorySynchronizationRequired && isDcFlushRequired) {
        PipeControlArgs args{};
        args.dcFlushEnable = true;
        args.workloadPartitionOffset = workloadPartitionIdOffsetEnable;

        if (cmdBuffer == nullptr) {
            DEBUG_BREAK_IF(commandStream == nullptr);
            cmdBuffer = commandStream->getSpaceForCmd<PIPE_CONTROL>();
        }
        MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(
            cmdBuffer,
            NEO::PostSyncMode::immediateData,
            idGpuAddress,
            hostFunctionId,
            args);
    } else {
        auto lowPart = getLowPart(hostFunctionId);
        auto highPart = getHighPart(hostFunctionId);
        bool storeQword = true;
        EncodeStoreMemory<GfxFamily>::programStoreDataImmCommand(commandStream,
                                                                 static_cast<MI_STORE_DATA_IMM *>(cmdBuffer),
                                                                 idGpuAddress,
                                                                 lowPart,
                                                                 highPart,
                                                                 storeQword,
                                                                 workloadPartitionIdOffsetEnable);
    }
}

template <typename GfxFamily>
void HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(LinearStream *commandStream, void *cmdBuffer, const HostFunctionStreamer &streamer, uint32_t partitionId) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    auto idGpuAddress = streamer.getHostFunctionIdGpuAddress(partitionId);
    auto waitValue = HostFunctionStatus::completed;

    EncodeSemaphore<GfxFamily>::programMiSemaphoreWaitCommand(commandStream,
                                                              cmdBuffer,
                                                              idGpuAddress,
                                                              waitValue,
                                                              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                              false,
                                                              true,
                                                              false,
                                                              false,
                                                              false);
}

} // namespace NEO
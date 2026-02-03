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

    auto partitionId = 0u; // get address of partition 0, other partitions will use workloadPartitionIdOffset
    auto idGpuAddress = streamer.getHostFunctionIdGpuAddress(partitionId);
    auto hostFunctionId = streamer.getNextHostFunctionIdAndIncrement();
    streamer.addHostFunction(hostFunctionId, std::move(hostFunction));

    bool workloadPartitionIdOffsetEnable = streamer.getActivePartitions() > 1;
    bool dcFlushPlatform = streamer.getDcFlushRequired();
    auto size = getSizeForHostFunctionIdProgramming(isMemorySynchronizationRequired, dcFlushPlatform);

    if (cmdBuffer == nullptr) {
        DEBUG_BREAK_IF(commandStream == nullptr);
        cmdBuffer = commandStream->getSpace(size);
    }

    bool programIdUsingPcWithDcFlush = usePipeControlForHostFunction(isMemorySynchronizationRequired, dcFlushPlatform);
    if (programIdUsingPcWithDcFlush) {
        PipeControlArgs args{};
        args.dcFlushEnable = true;
        args.workloadPartitionOffset = workloadPartitionIdOffsetEnable;
        MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(cmdBuffer,
                                                                   NEO::PostSyncMode::immediateData,
                                                                   idGpuAddress,
                                                                   hostFunctionId,
                                                                   args);
        return;
    }

    bool needMemoryFence = useMemoryFenceForHostFunction(isMemorySynchronizationRequired, dcFlushPlatform);
    if (needMemoryFence) {
        cmdBuffer = MemorySynchronizationCommands<GfxFamily>::programMemoryFenceRelease(cmdBuffer);
    }

    auto lowPart = getLowPart(hostFunctionId);
    auto highPart = getHighPart(hostFunctionId);
    bool storeQword = true;
    EncodeStoreMemory<GfxFamily>::programStoreDataImm(static_cast<MI_STORE_DATA_IMM *>(cmdBuffer),
                                                      idGpuAddress,
                                                      lowPart,
                                                      highPart,
                                                      storeQword,
                                                      workloadPartitionIdOffsetEnable);
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
                                                              false,
                                                              streamer.isUsingSemaphore64bCmd());
}

template <typename GfxFamily>
bool HostFunctionHelper<GfxFamily>::isMemorySynchronizationRequired() {
    bool memorySynchronizationRequired = true;
    if (NEO::debugManager.flags.UseMemorySynchronizationForHostFunction.get() != -1) {
        memorySynchronizationRequired = NEO::debugManager.flags.UseMemorySynchronizationForHostFunction.get() == 1;
    }
    return memorySynchronizationRequired;
}

template <typename GfxFamily>
bool HostFunctionHelper<GfxFamily>::usePipeControlForHostFunction(bool memorySynchronizationRequired, bool dcFlushPlatform) {
    return dcFlushPlatform && memorySynchronizationRequired;
}

template <typename GfxFamily>
bool HostFunctionHelper<GfxFamily>::useMemoryFenceForHostFunction(bool memorySynchronizationRequired, bool dcFlushPlatform) {
    return !dcFlushPlatform && memorySynchronizationRequired;
}

template <typename GfxFamily>
size_t HostFunctionHelper<GfxFamily>::getSizeForHostFunctionIdProgramming(bool memorySynchronizationRequired, bool dcFlushPlatform) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    bool usePipeControl = usePipeControlForHostFunction(memorySynchronizationRequired, dcFlushPlatform);
    bool isMemoryFenceRequired = useMemoryFenceForHostFunction(memorySynchronizationRequired, dcFlushPlatform);

    if (usePipeControl) {
        return sizeof(PIPE_CONTROL);
    }

    if constexpr (requires { typename GfxFamily::MI_MEM_FENCE; }) {
        using MI_MEM_FENCE = typename GfxFamily::MI_MEM_FENCE;

        if (isMemoryFenceRequired) {
            return sizeof(MI_MEM_FENCE) + sizeof(MI_STORE_DATA_IMM);
        }
    }

    return sizeof(MI_STORE_DATA_IMM);
}

} // namespace NEO
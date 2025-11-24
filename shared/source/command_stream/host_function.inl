/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/host_function.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
void HostFunctionHelper<GfxFamily>::programHostFunction(LinearStream &commandStream, HostFunctionStreamer &streamer, HostFunction &&hostFunction) {
    HostFunctionHelper<GfxFamily>::programHostFunctionId(&commandStream, nullptr, streamer, std::move(hostFunction));
    HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(&commandStream, nullptr, streamer);
}

template <typename GfxFamily>
void HostFunctionHelper<GfxFamily>::programHostFunctionId(LinearStream *commandStream, void *cmdBuffer, HostFunctionStreamer &streamer, HostFunction &&hostFunction) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    auto idGpuAddress = streamer.getHostFunctionIdGpuAddress();
    auto hostFunctionId = streamer.getNextHostFunctionIdAndIncrement();
    streamer.addHostFunction(hostFunctionId, std::move(hostFunction));

    auto lowPart = getLowPart(hostFunctionId);
    auto highPart = getHighPart(hostFunctionId);
    bool storeQword = true;

    EncodeStoreMemory<GfxFamily>::programStoreDataImmCommand(commandStream,
                                                             static_cast<MI_STORE_DATA_IMM *>(cmdBuffer),
                                                             idGpuAddress,
                                                             lowPart,
                                                             highPart,
                                                             storeQword,
                                                             false);
}

template <typename GfxFamily>
void HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(LinearStream *commandStream, void *cmdBuffer, const HostFunctionStreamer &streamer) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

    auto idGpuAddress = streamer.getHostFunctionIdGpuAddress();
    auto waitValue = HostFunctionStatus::completed;

    EncodeSemaphore<GfxFamily>::programMiSemaphoreWaitCommand(commandStream,
                                                              static_cast<MI_SEMAPHORE_WAIT *>(cmdBuffer),
                                                              idGpuAddress,
                                                              waitValue,
                                                              GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                              false,
                                                              true,
                                                              false,
                                                              false,
                                                              false);
}

} // namespace NEO
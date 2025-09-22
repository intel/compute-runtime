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
void HostFunctionHelper::programHostFunction(LinearStream &commandStream, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress, uint64_t userDataAddress) {

    HostFunctionHelper::programHostFunctionAddress<GfxFamily>(&commandStream, nullptr, hostFunctionData, userHostFunctionAddress);
    HostFunctionHelper::programHostFunctionUserData<GfxFamily>(&commandStream, nullptr, hostFunctionData, userDataAddress);
    HostFunctionHelper::programSignalHostFunctionStart<GfxFamily>(&commandStream, nullptr, hostFunctionData);
    HostFunctionHelper::programWaitForHostFunctionCompletion<GfxFamily>(&commandStream, nullptr, hostFunctionData);
}

template <typename GfxFamily>
void HostFunctionHelper::programHostFunctionAddress(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    auto hostFunctionAddressDst = reinterpret_cast<uint64_t>(hostFunctionData.entry);

    EncodeStoreMemory<GfxFamily>::programStoreDataImmCommand(commandStream,
                                                             static_cast<MI_STORE_DATA_IMM *>(cmdBuffer),
                                                             hostFunctionAddressDst,
                                                             getLowPart(userHostFunctionAddress),
                                                             getHighPart(userHostFunctionAddress),
                                                             true,
                                                             false);
}

template <typename GfxFamily>
void HostFunctionHelper::programHostFunctionUserData(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData, uint64_t userDataAddress) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    auto userDataAddressDst = reinterpret_cast<uint64_t>(hostFunctionData.userData);

    EncodeStoreMemory<GfxFamily>::programStoreDataImmCommand(commandStream,
                                                             static_cast<MI_STORE_DATA_IMM *>(cmdBuffer),
                                                             userDataAddressDst,
                                                             getLowPart(userDataAddress),
                                                             getHighPart(userDataAddress),
                                                             true,
                                                             false);
}

template <typename GfxFamily>
void HostFunctionHelper::programSignalHostFunctionStart(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    auto internalTagAddress = reinterpret_cast<uint64_t>(hostFunctionData.internalTag);
    EncodeStoreMemory<GfxFamily>::programStoreDataImmCommand(commandStream,
                                                             static_cast<MI_STORE_DATA_IMM *>(cmdBuffer),
                                                             internalTagAddress,
                                                             static_cast<uint32_t>(HostFunctionTagStatus::pending),
                                                             0u,
                                                             false,
                                                             false);
}

template <typename GfxFamily>
void HostFunctionHelper::programWaitForHostFunctionCompletion(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

    auto internalTagAddress = reinterpret_cast<uint64_t>(hostFunctionData.internalTag);
    EncodeSemaphore<GfxFamily>::programMiSemaphoreWaitCommand(commandStream,
                                                              static_cast<MI_SEMAPHORE_WAIT *>(cmdBuffer),
                                                              internalTagAddress,
                                                              static_cast<uint32_t>(HostFunctionTagStatus::completed),
                                                              GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                              false,
                                                              true,
                                                              false,
                                                              false,
                                                              false);
}

} // namespace NEO
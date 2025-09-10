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

    HostFunctionHelper::programHostFunctionUserData<GfxFamily>(commandStream, hostFunctionData, userHostFunctionAddress, userDataAddress);
    HostFunctionHelper::programSignalHostFunctionStart<GfxFamily>(commandStream, hostFunctionData);
    HostFunctionHelper::programWaitForHostFunctionCompletion<GfxFamily>(commandStream, hostFunctionData);
}

template <typename GfxFamily>
void HostFunctionHelper::programHostFunctionUserData(LinearStream &commandStream, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress, uint64_t userDataAddress) {

    auto hostFunctionAddressDst = reinterpret_cast<uint64_t>(hostFunctionData.entry);
    auto userDataAddressDst = reinterpret_cast<uint64_t>(hostFunctionData.userData);

    EncodeStoreMemory<GfxFamily>::programStoreDataImm(commandStream,
                                                      hostFunctionAddressDst,
                                                      getLowPart(userHostFunctionAddress),
                                                      getHighPart(userHostFunctionAddress),
                                                      true,
                                                      false,
                                                      nullptr);

    EncodeStoreMemory<GfxFamily>::programStoreDataImm(commandStream,
                                                      userDataAddressDst,
                                                      getLowPart(userDataAddress),
                                                      getHighPart(userDataAddress),
                                                      true,
                                                      false,
                                                      nullptr);
}

template <typename GfxFamily>
void HostFunctionHelper::programSignalHostFunctionStart(LinearStream &commandStream, const HostFunctionData &hostFunctionData) {

    auto internalTagAddress = reinterpret_cast<uint64_t>(hostFunctionData.internalTag);
    EncodeStoreMemory<GfxFamily>::programStoreDataImm(commandStream,
                                                      internalTagAddress,
                                                      static_cast<uint32_t>(HostFunctionTagStatus::pending),
                                                      0u,
                                                      false,
                                                      false,
                                                      nullptr);
}

template <typename GfxFamily>
void HostFunctionHelper::programWaitForHostFunctionCompletion(LinearStream &commandStream, const HostFunctionData &hostFunctionData) {

    auto internalTagAddress = reinterpret_cast<uint64_t>(hostFunctionData.internalTag);
    EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(
        commandStream,
        internalTagAddress,
        static_cast<uint32_t>(HostFunctionTagStatus::completed),
        GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
        false,
        false,
        false,
        false,
        nullptr);
}

} // namespace NEO

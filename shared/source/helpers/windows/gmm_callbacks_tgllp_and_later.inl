/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/os_interface/windows/wddm_device_command_stream.h"

namespace NEO {

template <typename GfxFamily>
long __stdcall DeviceCallbacks<GfxFamily>::notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    auto csr = reinterpret_cast<CommandStreamReceiverHw<GfxFamily> *>(csrHandle);

    if (obtainCsrTypeFromIntegerValue(debugManager.flags.SetCommandStreamReceiver.get(), CommandStreamReceiverType::hardware) == CommandStreamReceiverType::hardwareWithAub) {
        auto csrWithAub = static_cast<CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<GfxFamily>> *>(csr);
        auto aubCsr = static_cast<AUBCommandStreamReceiverHw<GfxFamily> *>(csrWithAub->aubCSR.get());
        if (allocate) {
            AllocationView externalAllocation(gfxAddress, gfxSize);
            aubCsr->makeResidentExternal(externalAllocation);
        } else {
            aubCsr->makeNonResidentExternal(gfxAddress);
        }
    }

    return 1;
}

template <typename GfxFamily>
int __stdcall TTCallbacks<GfxFamily>::writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset) {
    auto csr = reinterpret_cast<CommandStreamReceiverHw<GfxFamily> *>(queueHandle);

    LriHelper<GfxFamily>::program(&csr->getCS(0),
                                  static_cast<uint32_t>(regOffset & 0xFFFFFFFF),
                                  static_cast<uint32_t>(l3GfxAddress & 0xFFFFFFFF),
                                  true,
                                  false);

    LriHelper<GfxFamily>::program(&csr->getCS(0),
                                  static_cast<uint32_t>(regOffset >> 32),
                                  static_cast<uint32_t>(l3GfxAddress >> 32),
                                  true,
                                  false);

    return 1;
}

} // namespace NEO

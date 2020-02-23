/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "opencl/source/os_interface/windows/wddm_device_command_stream.h"

namespace NEO {

template <typename GfxFamily>
long __stdcall DeviceCallbacks<GfxFamily>::notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    auto csr = reinterpret_cast<CommandStreamReceiverHw<GfxFamily> *>(csrHandle);

    if (DebugManager.flags.SetCommandStreamReceiver.get() == CSR_HW_WITH_AUB) {
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

    auto lri1 = LriHelper<GfxFamily>::program(&csr->getCS(0),
                                              static_cast<uint32_t>(regOffset & 0xFFFFFFFF),
                                              static_cast<uint32_t>(l3GfxAddress & 0xFFFFFFFF));
    lri1->setMmioRemapEnable(true);

    auto lri2 = LriHelper<GfxFamily>::program(&csr->getCS(0),
                                              static_cast<uint32_t>(regOffset >> 32),
                                              static_cast<uint32_t>(l3GfxAddress >> 32));
    lri2->setMmioRemapEnable(true);

    return 1;
}

} // namespace NEO

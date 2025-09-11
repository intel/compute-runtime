/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/gmm_helper/gmm_callbacks_base.inl"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/os_interface/windows/wddm_device_command_stream.h"

namespace NEO {

template <typename GfxFamily>
long __stdcall GmmCallbacks<GfxFamily>::notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    auto csr = reinterpret_cast<CommandStreamReceiverHw<GfxFamily> *>(csrHandle);

    auto csrWithAub = static_cast<CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<GfxFamily>> *>(csr);
    auto aubCsr = static_cast<AUBCommandStreamReceiverHw<GfxFamily> *>(csrWithAub->aubCSR.get());
    if (allocate) {
        AllocationView externalAllocation(gfxAddress, gfxSize);
        aubCsr->makeResidentExternal(externalAllocation);
    } else {
        aubCsr->makeNonResidentExternal(gfxAddress);
    }

    return 1;
}

template <>
void populateFactoryTable<GmmCallbacks<Family>>() {
    UNRECOVERABLE_IF(!isInRange(gfxCore, writeL3AddressFuncFactory));
    writeL3AddressFuncFactory[gfxCore] = GmmCallbacks<Family>::writeL3Address;

    UNRECOVERABLE_IF(!isInRange(gfxCore, notifyAubCaptureFuncFactory));
    notifyAubCaptureFuncFactory[gfxCore] = GmmCallbacks<Family>::notifyAubCapture;
}

template struct GmmCallbacks<Family>;

} // namespace NEO

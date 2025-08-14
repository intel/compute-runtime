/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
int __stdcall GmmCallbacks<GfxFamily>::writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset) {
    auto csr = reinterpret_cast<CommandStreamReceiver *>(queueHandle);

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

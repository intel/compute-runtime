/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_callbacks_base.inl"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {

template <typename GfxFamily>
long __stdcall GmmCallbacks<GfxFamily>::notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    return 1;
}

template <>
void populateFactoryTable<GmmCallbacks<Family>>() {
    UNRECOVERABLE_IF(!isInRange(gfxCore, writeL3AddressFuncFactory));
    writeL3AddressFuncFactory[gfxCore] = GmmCallbacks<Family>::writeL3Address;
}

template struct GmmCallbacks<Family>;

} // namespace NEO

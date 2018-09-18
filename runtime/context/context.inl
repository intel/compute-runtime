/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

namespace OCLRT {

template <typename Sharing>
void Context::registerSharing(Sharing *sharing) {
    UNRECOVERABLE_IF(!sharing);
    this->sharingFunctions[Sharing::sharingId].reset(sharing);
}

template <typename Sharing>
Sharing *Context::getSharing() {
    if (Sharing::sharingId >= sharingFunctions.size()) {
        return nullptr;
    }

    return reinterpret_cast<Sharing *>(sharingFunctions[Sharing::sharingId].get());
}
} // namespace OCLRT

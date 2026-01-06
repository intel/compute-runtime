/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/windows/gmm_memory.h"

namespace NEO {
GmmMemory::GmmMemory(GmmClientContext *gmmClientContext) : clientContext(*gmmClientContext->getHandle()) {
}
}; // namespace NEO

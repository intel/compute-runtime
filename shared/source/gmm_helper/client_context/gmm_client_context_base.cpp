/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {
GmmClientContext::GmmClientContext() = default;
GmmClientContext::~GmmClientContext() = default;

GMM_CLIENT_CONTEXT *GmmClientContext::getHandle() const {
    return clientContext.get();
}

void GmmClientContext::setHandleAllocator(std::unique_ptr<GmmHandleAllocator> allocator) {
    this->handleAllocator = std::move(allocator);
}

} // namespace NEO
